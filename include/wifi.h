#ifndef __WIFI_H__
#define __WIFI_H__

#include "flags.h"

#if WIFI_ENABLED

bool connect_wifi(
    char *wifi_ssid,
    char *wifi_password);
bool connect_tcp_server(
    uint32_t tcp_server_ipv4_addr,
    uint32_t tcp_server_port);

#endif // WIFI_ENABLED

#endif // __WIFI_H__