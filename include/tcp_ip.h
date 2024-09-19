#ifndef __TCP_IP_H__
#define __TCP_IP_H__

// Include custom WiFi API
#include "wifi.h"

#if WIFI_ENABLED

// To create a TCP connection with a Windows computer connected to the same AP, try these steps:
// 1. Install https://nmap.org/download#windows
// 2. Make sure to have it enabled for public networks. Just private networks didn't seem to work for me.
//    You can do this during the install, and manually later via Windows Defender Firewall with Advanced Security -> Inbound Rules ->
//    Name: ncat, profile: Public, Protocol: TCP -> right click to enable, you can limit the ports from this menu if you'd like
// 3. Get the IPv4 address of your computer, #define TCP_SERVER_IPV4_ADDR as it.
//    You can find this do this by running "ipconfig" in Powershell -> Wireless LAN adapter Wi-Fi: -> IPv4 Address
// 3. Open a TCP port on your computer, #define TCP_SERVER_PORT as it
//    In PowerShell, use the command "ncat -l SOME_PORT_NUMBER", launch or restart the ESP32,
//    and type something in the Powershell for it to transit to the ESP32.

bool tcp_start(
    uint32_t tcp_server_ipv4_addr,
    uint32_t tcp_server_port);
bool tcp_free();
bool tcp_send(
    void *packet,
    size_t num_packet_bytes,
    int flags = 0);

#endif // WIFI_ENABLED

#endif // __TCP_IP_H__