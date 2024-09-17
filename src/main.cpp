// Helpful resources:
// 1. Set up VSCode environment with automatic pin detection, get configuration tips.
//     https://www.youtube.com/watch?v=XLQa1sX9KIk
//     If you just want to use Linux, skip this video.
//     Use Arduino as framework to get access to Arduino helpers (and most tutorials are written in it)
//     >Tasks: Run Build Task -> PlatformIO: Build
//     >PlatformIO: Upload
//     >PlatformIO: Serial Monitor
// 2. Create your first blink-LED program on an ESP32 in C/C++, learn some basic RTOS functions.
//     https://www.youtube.com/watch?v=dOVjb2wXI84
// 3. ESP32-WROOM-32 data sheet, pins, etc.
//     https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32_datasheet_en.pdf

// Include custom Button class implementation
#include "button.h"
// Include custom Menu class implementation
#include "menu.h"
// Include custom debug macros
#include "flags.h"

#if WIFI_ENABLED

// Include custom wifi API
#include "wifi.h"

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

// To create a TCP connection with a Windows computer connected to the same WiFi router, try these steps:
// 1. Install https://nmap.org/download#windows
// 2. Make sure to have it enabled for public networks. Just private networks didn't seem to work for me.
//    You can do this during the install, and manually later via Windows Defender Firewall with Advanced Security -> Inbound Rules ->
//    Name: ncat, profile: Public, Protocol: TCP -> right click to enable, you can limit the ports from this menu if you'd like
// 3. Get the IPv4 address of your computer, #define TCP_SERVER_IPV4_ADDR as it.
//    You can find this do this by running "ipconfig" in Powershell -> Wireless LAN adapter Wi-Fi: -> IPv4 Address
// 3. Open a TCP port on your computer, #define TCP_SERVER_PORT as it
//    In PowerShell, use the command "ncat -l SOME_PORT_NUMBER", launch or restart the ESP32,
//    and type something in the Powershell for it to transit to the ESP32.

#endif // WIFI_ENABLED

// =========================== //
// Initialize and start device //
// =========================== //

// Start program
void setup()
{
#if PRINT
    // Initialize Arduino serial console
    Serial.begin(/* unsigned long baud = */ 115200);
    // Wait for serial port to connect
    while(!Serial);
#endif

    // Initialze menu and its input queue/task
    init_menu();
    // Initialize GPIO buttons and their interrupts
    init_buttons();

#if WIFI_ENABLED
    if(true == wifi_start(
        /* char *wifi_ssid = */ WIFI_SSID,
        /* char *wifi_password = */ WIFI_PASSWORD))
    {
        tcp_start(
            /* uint32_t tcp_server_ipv4_addr = */ TCP_SERVER_IPV4_ADDR,
            /* uint32_t tcp_server_port = */ TCP_SERVER_PORT);
    }
#endif // WIFI_ENABLED
}

// Don't have any need for a loop that runs forever, because we're using FreeRTOS tasks,
// but this function needs to be defined to make the Arduino framework happy
void loop()
{
}