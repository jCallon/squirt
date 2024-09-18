#ifndef __FLAGS_H__
#define __FLAGS_H__

// Define whether you want to compile the the Arduino Serial console, for example,
// if you're not going to be connected to a computer, and you'll be unable to receive
// anything it outputs, so you don't need it
#define PRINT 1

#if PRINT
#include <Arduino.h>
#define s_print(...) Serial.print(__VA_ARGS__)
#define s_println(...) Serial.println(__VA_ARGS__)
#else // PRINT
#define s_print(...) (void) 0
#define s_println(...) (void) 0
#endif // PRINT

// Define whether you want each task to print out its stack usage.
// This is useful for development and debugging, when you're creating tasks and trying to optimize stack size,
// but not once you've already hit your optimized value, and are in production
#define PRINT_STACK_DIAGNOSTICS 1

#if PRINT && PRINT_STACK_DIAGNOSTICS
#define PRINT_STACK_USAGE() Serial.print(pcTaskGetName(/* TaskHandle_t xTaskToQuery = */ NULL)); \
    Serial.print(" high water mark (words): "); \
    Serial.println(uxTaskGetStackHighWaterMark(/* TaskHandle_t xTask = */ NULL))
#else // PRINT_STACK_DIAGNOSTICS
#define PRINT_STACK_USAGE (void) 0
#endif // PRINT_STACK_DIAGNOSTICS

// Define whether you want to compile the code to WiFi-enable this project, which takes more memory and power
#define WIFI_ENABLED 1

// Return false and give a helpful debug message if a check failed
// Created by looking at ESP_ERROR_CHECK_WITHOUT_ABORT(...)
// TODO: Should this printing to the ESP32 serial console be disabled if PRINT is disabled?
//       Anywhere besides wifi.cpp that should use this?
#define ESP_ERROR_RETURN_FALSE_IF_FAILED(check_status, check) \
    if (ESP_OK != ((check_status) = (check))) \
    { \
        _esp_error_check_failed_without_abort(check_status, __FILE__, __LINE__, __ASSERT_FUNC, #check); \
        return false; \
    }

// Record false in an overall status and give a helpful debug message if a check failed
// Created by looking at ESP_ERROR_CHECK_WITHOUT_ABORT(...)
// TODO: Should this printing to the ESP32 serial console be disabled if PRINT is disabled?
//       Anywhere besides wifi.cpp that should use this?
#define ESP_ERROR_RECORD_FALSE_IF_FAILED(overall_status, check_status, check) \
    if (ESP_OK != ((check_status) = (check))) \
    { \
        _esp_error_check_failed_without_abort(check_status, __FILE__, __LINE__, __ASSERT_FUNC, #check); \
        overall_status = false; \
    }

#endif // __FLAGS_H__