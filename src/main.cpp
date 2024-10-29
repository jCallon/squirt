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
// Include custom TCP/IP API
#include "tcp_ip.h"
// Include custom storage API
#include "storage.h"

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

    // Intialize storage, if it went wrong, don't care enough to crash
    (void) storage_init(/* bool reinit = */ false);

    // Initialize menu and its input queue/task
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