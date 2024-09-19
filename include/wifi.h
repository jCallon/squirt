#ifndef __WIFI_H__
#define __WIFI_H__

// Include custom debug macros and compile flags
#include "flags.h"

#if WIFI_ENABLED

// Include WiFi/TCP/IP credentials
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

bool wifi_start(
    char *wifi_ssid,
    char *wifi_password);
bool wifi_free();

#endif // WIFI_ENABLED

#endif // __WIFI_H__