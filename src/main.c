// Helpful resources:
// 1. Set up VSCode environment with automatic pin detection, get configuration tips.
//     https://www.youtube.com/watch?v=XLQa1sX9KIk
//     If you just want to use Linux, skip this video.
//     This project uses espidf instead of Ardiuno for the framework, for C/C++ RTOS practice.
//     >Tasks: Run Build Task -> PlatformIO: Build
//     >PlatformIO: Upload
//     >PlatformIO: Serial Monitor
// 2. Create your first blink-LED program on an ESP32 in C/C++, learn some basic RTOS functions.
//     https://www.youtube.com/watch?v=dOVjb2wXI84

// Include FreeRTOS OS API
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task handling API
#include "freeRTOS/task.h"
// Include ESP32 logging API
#include "esp_log.h"

// Create macro for converting milliseconds to clock ticks
#define MS_TO_CLOCK_TICK(ms) ms / portTICK_PERIOD_MS

// TODO:
// - Print to the serial console when a button is pressed
// - Print "Hello world" on a I2C display

void app_main()
{
    // Get name of this task
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/search.html?q=pcTaskGetName
    char *task_name = pcTaskGetName(/*TaskHandle_t xTaskToQuery = */ NULL);

    // To not trigger the watchdog and keep the device running, create a loop that yields the process
    // Every loop iteration, log "Hello world!" to information serial console, then yield for 1 second
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/search.html?q=esp_logi
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/search.html?q=vTaskDelay
    while(1)
    {
        ESP_LOGI(/*const char *tag = */ task_name, /*const char *format = */ "Hello world!");
        vTaskDelay(/*const TickType_t xTicksToDelay = */MS_TO_CLOCK_TICK(1000));
    }
}