/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

#include "credentials.h"

LOG_MODULE_REGISTER(credentials);

/* NVS key IDs */
#define NVS_ID_SERVER      1
#define NVS_ID_AUTH_TOKEN  2
#define NVS_ID_TEMPLATE_ID 3
#define NVS_ID_APN         4
#define NVS_ID_APN_USER    5
#define NVS_ID_APN_PASS    6

static struct nvs_fs fs;

static char server[CRED_SERVER_MAX_LEN];
static char auth_token[CRED_AUTH_TOKEN_MAX_LEN];
static char template_id[CRED_TEMPLATE_MAX_LEN];
static char apn[CRED_APN_MAX_LEN];
static char apn_user[CRED_APN_USER_MAX_LEN];
static char apn_pass[CRED_APN_PASS_MAX_LEN];

/* ── NVS init ────────────────────────────────────────────────────────────── */

static int nvs_storage_init(void)
{
	const struct flash_area *fa;
	int err;

	err = flash_area_open(FIXED_PARTITION_ID(storage_partition), &fa);
	if (err) {
		LOG_ERR("Failed to open storage partition: %d", err);
		return err;
	}

	fs.flash_device = flash_area_get_device(fa);
	if (!device_is_ready(fs.flash_device)) {
		LOG_ERR("Flash device not ready");
		flash_area_close(fa);
		return -ENODEV;
	}

	fs.offset       = fa->fa_off;
	fs.sector_size  = 4096;
	fs.sector_count = (uint16_t)(fa->fa_size / fs.sector_size);

	flash_area_close(fa);

	err = nvs_mount(&fs);
	if (err) {
		LOG_ERR("nvs_mount() failed: %d", err);
	}
	return err;
}

static void read_nvs_str(uint16_t id, char *buf, size_t buf_size)
{
	ssize_t rc = nvs_read(&fs, id, buf, buf_size);

	if (rc <= 0) {
		buf[0] = '\0';
	}
}

/* ── credentials_init ────────────────────────────────────────────────────── */

int credentials_init(void)
{
	int err = nvs_storage_init();

	if (err) {
		return err;
	}

	read_nvs_str(NVS_ID_SERVER,      server,      sizeof(server));
	read_nvs_str(NVS_ID_AUTH_TOKEN,  auth_token,  sizeof(auth_token));
	read_nvs_str(NVS_ID_TEMPLATE_ID, template_id, sizeof(template_id));
	read_nvs_str(NVS_ID_APN,         apn,         sizeof(apn));
	read_nvs_str(NVS_ID_APN_USER,    apn_user,    sizeof(apn_user));
	read_nvs_str(NVS_ID_APN_PASS,    apn_pass,    sizeof(apn_pass));

	/* Apply compile-time defaults for optional fields if not stored */
	if (server[0] == '\0') {
		strncpy(server, CONFIG_BLYNK_SERVER, sizeof(server) - 1);
	}

	if (auth_token[0] == '\0') {
		LOG_WRN("No auth token stored. Use shell to provision:");
		LOG_WRN("  cred token <your-auth-token>");
		LOG_WRN("  cred server <host>      (optional, default: %s)", CONFIG_BLYNK_SERVER);
		LOG_WRN("  cred template <id>      (optional)");
		LOG_WRN("  cred apn <apn>          (optional, for networks needing manual APN)");
		LOG_WRN("  cred apn_user <user>    (optional, APN username)");
		LOG_WRN("  cred apn_pass <pass>    (optional, APN password)");
		LOG_WRN("  cred show               (show current values)");
		LOG_WRN("  cred clear              (erase all credentials)");
		return -ENOENT;
	}

	LOG_INF("Credentials loaded:");
	LOG_INF("  Server:      %s", server);
	LOG_INF("  Template ID: %s", template_id[0] ? template_id : "(none)");
	LOG_INF("  Auth token:  %.8s…", auth_token);
	if (apn[0] != '\0') {
		LOG_INF("  APN:         %s", apn);
		LOG_INF("  APN user:    %s", apn_user[0] ? apn_user : "(none)");
	}
	return 0;
}

/* ── Accessors ───────────────────────────────────────────────────────────── */

const char *credentials_get_server(void)      { return server; }
const char *credentials_get_auth_token(void)  { return auth_token; }
const char *credentials_get_template_id(void) { return template_id; }
const char *credentials_get_apn(void)         { return apn; }
const char *credentials_get_apn_user(void)    { return apn_user; }
const char *credentials_get_apn_pass(void)    { return apn_pass; }

/* ── Shell commands ──────────────────────────────────────────────────────── */

static int cmd_cred_token(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: cred token <auth-token>");
		return -EINVAL;
	}
	strncpy(auth_token, argv[1], sizeof(auth_token) - 1);
	auth_token[sizeof(auth_token) - 1] = '\0';
	nvs_write(&fs, NVS_ID_AUTH_TOKEN, auth_token, strlen(auth_token) + 1);
	shell_print(sh, "Auth token saved. Rebooting…");
	k_sleep(K_MSEC(200));
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

static int cmd_cred_server(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: cred server <host>");
		return -EINVAL;
	}
	strncpy(server, argv[1], sizeof(server) - 1);
	server[sizeof(server) - 1] = '\0';
	nvs_write(&fs, NVS_ID_SERVER, server, strlen(server) + 1);
	shell_print(sh, "Server saved: %s", server);
	return 0;
}

static int cmd_cred_template(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: cred template <template-id>");
		return -EINVAL;
	}
	strncpy(template_id, argv[1], sizeof(template_id) - 1);
	template_id[sizeof(template_id) - 1] = '\0';
	nvs_write(&fs, NVS_ID_TEMPLATE_ID, template_id, strlen(template_id) + 1);
	shell_print(sh, "Template ID saved: %s", template_id);
	return 0;
}

static int cmd_cred_apn(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: cred apn <apn-string>");
		return -EINVAL;
	}
	strncpy(apn, argv[1], sizeof(apn) - 1);
	apn[sizeof(apn) - 1] = '\0';
	nvs_write(&fs, NVS_ID_APN, apn, strlen(apn) + 1);
	shell_print(sh, "APN saved: %s", apn);
	return 0;
}

static int cmd_cred_apn_user(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: cred apn_user <username>");
		return -EINVAL;
	}
	strncpy(apn_user, argv[1], sizeof(apn_user) - 1);
	apn_user[sizeof(apn_user) - 1] = '\0';
	nvs_write(&fs, NVS_ID_APN_USER, apn_user, strlen(apn_user) + 1);
	shell_print(sh, "APN username saved.");
	return 0;
}

static int cmd_cred_apn_pass(const struct shell *sh, size_t argc, char **argv)
{
	if (argc < 2) {
		shell_error(sh, "Usage: cred apn_pass <password>");
		return -EINVAL;
	}
	strncpy(apn_pass, argv[1], sizeof(apn_pass) - 1);
	apn_pass[sizeof(apn_pass) - 1] = '\0';
	nvs_write(&fs, NVS_ID_APN_PASS, apn_pass, strlen(apn_pass) + 1);
	shell_print(sh, "APN password saved.");
	return 0;
}

static int cmd_cred_show(const struct shell *sh, size_t argc, char **argv)
{
	shell_print(sh, "Server:      %s", server[0]      ? server      : "(not set)");
	shell_print(sh, "Template ID: %s", template_id[0] ? template_id : "(not set)");
	shell_print(sh, "Auth token:  %s", auth_token[0]  ? auth_token  : "(not set)");
	shell_print(sh, "APN:         %s", apn[0]         ? apn         : "(not set)");
	shell_print(sh, "APN user:    %s", apn_user[0]    ? apn_user    : "(not set)");
	shell_print(sh, "APN pass:    %s", apn_pass[0]    ? "(set)"     : "(not set)");
	return 0;
}

static int cmd_cred_clear(const struct shell *sh, size_t argc, char **argv)
{
	nvs_delete(&fs, NVS_ID_SERVER);
	nvs_delete(&fs, NVS_ID_AUTH_TOKEN);
	nvs_delete(&fs, NVS_ID_TEMPLATE_ID);
	nvs_delete(&fs, NVS_ID_APN);
	nvs_delete(&fs, NVS_ID_APN_USER);
	nvs_delete(&fs, NVS_ID_APN_PASS);
	server[0] = auth_token[0] = template_id[0] = '\0';
	apn[0] = apn_user[0] = apn_pass[0] = '\0';
	shell_print(sh, "Credentials cleared. Rebooting…");
	k_sleep(K_MSEC(200));
	sys_reboot(SYS_REBOOT_COLD);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(cred_cmds,
	SHELL_CMD_ARG(token,    NULL, "Set Blynk auth token (reboots)",    cmd_cred_token,    2, 0),
	SHELL_CMD_ARG(server,   NULL, "Set Blynk server host",             cmd_cred_server,   2, 0),
	SHELL_CMD_ARG(template, NULL, "Set Blynk template ID",             cmd_cred_template, 2, 0),
	SHELL_CMD_ARG(apn,      NULL, "Set LTE APN",                       cmd_cred_apn,      2, 0),
	SHELL_CMD_ARG(apn_user, NULL, "Set APN username",                  cmd_cred_apn_user, 2, 0),
	SHELL_CMD_ARG(apn_pass, NULL, "Set APN password",                  cmd_cred_apn_pass, 2, 0),
	SHELL_CMD_ARG(show,     NULL, "Show current credentials",          cmd_cred_show,     1, 0),
	SHELL_CMD_ARG(clear,    NULL, "Erase all credentials (reboots)",   cmd_cred_clear,    1, 0),
	SHELL_SUBCMD_SET_END
);
SHELL_CMD_REGISTER(cred, &cred_cmds, "Blynk credential management", NULL);
