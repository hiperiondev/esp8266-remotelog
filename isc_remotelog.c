/*
 * This file is part of the esp-iot-secure-core distribution
 * Copyright (c) 2020 Emiliano Augusto Gonzalez (comercial@hiperion.com.ar)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <esp_err.h>
#include <esp_log.h>
#include "lwip/sockets.h"
#include "isc_remotelog.h"
#include "isc_project.h"
#define TAG "isc_remotelog"

static int isc_log_serv_sockfd = -1;
static int log_sockfd = -1;
static struct sockaddr_in isc_log_srv_addr, log_cli_addr;
static putchar_like_t orig_putchar_cb;

static int isc_remote_log_putchar_cb(int c) {
    int err = send(log_sockfd, &c, 1, 0);
    return ((err == -1) ? EOF : c);
}

int isc_remotelog_init(long timeout_sec) {
    int ret;
    struct timeval timeout = {
            .tv_sec = timeout_sec,
            .tv_usec = 0
    };

    memset(&isc_log_srv_addr, 0, sizeof(isc_log_srv_addr));
    memset(&log_cli_addr, 0, sizeof(log_cli_addr));

    isc_log_srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    isc_log_srv_addr.sin_family = AF_INET;
    isc_log_srv_addr.sin_port = htons(ISC_CORE_REMOTELOG_PORT);

    isc_log_serv_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (isc_log_serv_sockfd < 0) {
        ESP_LOGE(TAG, "Failed to create socket, fd value: %d", isc_log_serv_sockfd);
        return isc_log_serv_sockfd;
    }

    int reuse_option = 1;
    ret = setsockopt(isc_log_serv_sockfd, SOL_SOCKET, SO_REUSEADDR, (char* )&reuse_option,
            sizeof(reuse_option));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to set reuse: %d, %s", ret, strerror(errno));
        isc_remotelog_stop();
        return ret;
    }

    ret = bind(isc_log_serv_sockfd, (struct sockaddr* )&isc_log_srv_addr, sizeof(isc_log_srv_addr));
    if (ret < 0) {
        ESP_LOGE(TAG, "Failed to bind the port: %d, %s", ret, strerror(errno));
        isc_remotelog_stop();
        return ret;
    }

    ret = listen(isc_log_serv_sockfd, 1);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to listen: %d", ret);
        return ret;
    }

    ret = setsockopt(isc_log_serv_sockfd, SOL_SOCKET, SO_RCVTIMEO, (char* )&timeout,
            sizeof(timeout));
    if (ret < 0) {
        ESP_LOGI(TAG, "Setting receive timeout failed");
        isc_remotelog_stop();
        return ret;
    }

    ret = setsockopt(isc_log_serv_sockfd, SOL_SOCKET, SO_SNDTIMEO, (char* )&timeout,
            sizeof(timeout));
    if (ret < 0) {
        ESP_LOGI(TAG, "Setting send timeout failed");
        isc_remotelog_stop();
        return ret;
    }

    ESP_LOGI(TAG, "Remote log started, waiting connection for %d seconds", (int)timeout_sec);

    size_t cli_addr_len = sizeof(log_cli_addr);

    log_sockfd = accept(isc_log_serv_sockfd, (struct sockaddr* )&log_cli_addr, &cli_addr_len);
    if (log_sockfd < 0) {
        ESP_LOGE(TAG, "Failed to accept: %d, %s", ret, strerror(errno));
        isc_remotelog_stop();
        return log_sockfd;
    }

    send(log_sockfd, "\nisc_remotelog >\n\n", 18, 0);
    ESP_LOGI(TAG, "Connected");

    orig_putchar_cb = esp_log_set_putchar(isc_remote_log_putchar_cb);

    return ESP_OK;
}

int isc_remotelog_stop() {
    int ret = close(isc_log_serv_sockfd);
    if (ret != 0) {
        ESP_LOGE(TAG, "Cannot close the socket: %d", ret);
        return ret;
    }

    if (orig_putchar_cb != NULL) {
        esp_log_set_putchar(orig_putchar_cb);
    }

    ESP_LOGI(TAG, "Remote log stopped");
    return ESP_OK;
}
