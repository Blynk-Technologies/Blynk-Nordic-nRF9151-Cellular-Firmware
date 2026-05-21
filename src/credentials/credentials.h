/*
 * Copyright (c) 2020 Circuit Dojo LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef CREDENTIALS_H
#define CREDENTIALS_H

#define CRED_SERVER_MAX_LEN      64
#define CRED_AUTH_TOKEN_MAX_LEN  64
#define CRED_TEMPLATE_MAX_LEN    32
#define CRED_APN_MAX_LEN         64
#define CRED_APN_USER_MAX_LEN    32
#define CRED_APN_PASS_MAX_LEN    32

/**
 * @brief Initialise credential storage.
 *
 * Mounts NVS and loads all stored values into RAM. If the auth token is
 * absent, logs provisioning instructions and returns -ENOENT so the caller
 * can pause until the shell `cred token` command has been run and the device
 * rebooted.
 *
 * @return 0 on success, -ENOENT if auth token is not yet provisioned,
 *         or a negative errno on storage failure.
 */
int credentials_init(void);

/**
 * @brief Return the stored MQTT broker hostname.
 *
 * Falls back to CONFIG_BLYNK_SERVER if not overridden via the shell.
 */
const char *credentials_get_server(void);

/**
 * @brief Return the stored Blynk auth token.
 *
 * Valid only after credentials_init() returns 0.
 */
const char *credentials_get_auth_token(void);

/**
 * @brief Return the stored Blynk template ID, or "" if not set.
 */
const char *credentials_get_template_id(void);

/**
 * @brief Return the stored LTE APN string, or "" if not set.
 *
 * When non-empty, the caller should apply it via AT+CGDCONT before
 * calling lte_lc_connect().
 */
const char *credentials_get_apn(void);

/**
 * @brief Return the stored APN username, or "" if not set.
 */
const char *credentials_get_apn_user(void);

/**
 * @brief Return the stored APN password, or "" if not set.
 */
const char *credentials_get_apn_pass(void);

#endif /* CREDENTIALS_H */
