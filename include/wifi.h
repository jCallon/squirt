#ifndef __WIFI_H__
#define __WIFI_H__

#include "flags.h"

#if WIFI_ENABLED

bool wifi_start(
    char *wifi_ssid,
    char *wifi_password);
bool wifi_free();

bool tcp_start(
    uint32_t tcp_server_ipv4_addr,
    uint32_t tcp_server_port);
bool tcp_free();
bool tcp_send(
    void *packet,
    size_t num_packet_bytes,
    int flags = 0);

#endif // WIFI_ENABLED

#endif // __WIFI_H__