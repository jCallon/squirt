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
#endif

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
    if(true == connect_wifi())
    {
        connect_tcp_server();
    }
#endif
}

// Don't have any need for a loop that runs forever, because we're using FreeRTOS tasks,
// but this function needs to be defined to make the Arduino framework happy
void loop()
{
}