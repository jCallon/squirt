#ifndef __WIFI_H__
#define __WIFI_H__

// Include custom debug macros
#include "flags.h"

#if WIFI_ENABLED

// Include WiFi credentials
// TODO: Is there a better way to do this? Do I care? Eventually I may implement just being able to search among many WiFi networks.
#include "credentials.h"
#if !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(TCP_SERVER_IPV4_ADDR) || !defined(TCP_SERVER_PORT)
#error Please make a file called credentials.h in the include directory.\n\
It should look like this. Never post your credentials.h somewhere public.\n\
#ifndef __CREDENTIALS_H__\n\
#define __CREDENTIALS_H__\n\
\n\
// Nickname for WiFi AP to connect to\n\
#define WIFI_SSID "My WiFi network"\n\
// Password for WiFi AP to connect to\n\
#define WIFI_PASSWORD "my_wifi_password"\n\
// IPv4 address of TCP server to connect to, represented as hexidecimal 32 bit integer\n\
// ex: The address 1.2.3.4, because each number between the dots is a 1-byte value,\n\
// there are 4 numbers, and network order is big-endian, is equal to 0x04030201.\n\
#define TCP_SERVER_IPV4_ADDR 0x04030201\n\
// The port number for the TCP server to connect to\n\
#define TCP_SERVER_PORT 12345\n\
\n\
#endif // __CREDENTIALS_H__"
#endif // !defined(WIFI_SSID) || !defined(WIFI_PASSWORD) || !defined(TCP_SERVER_IPV4_ADDR) || !defined(TCP_SERVER_PORT)

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