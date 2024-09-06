#ifndef __FLAGS_H__
#define __FLAGS_H__

// Define whether you want to compile the the Arduino Serial console, for example,
// if you're not going to be connected to a computer, and you'll be unable to receive anythin it output, you don't need it
#define PRINT 1

#if PRINT
#include <Arduino.h>
#define s_print(...) Serial.print(__VA_ARGS__)
#define s_println(...) Serial.println(__VA_ARGS__)
#else
#define s_print(...) (void) 0
#define s_println(...) (void) 0
#endif

// Define whether you want each task to print out its stack usage.
// This is useful for development and debugging, when you're creating tasks and trying to optimize stack size,
// but not once you've already hit your optmized value, and are in production
#define PRINT_STACK_DIAGNOSTICS 1

#if PRINT && PRINT_STACK_DIAGNOSTICS
#define PRINT_STACK_USAGE() Serial.print(pcTaskGetName(/* TaskHandle_t xTaskToQuery = */ NULL)); \
    Serial.print(" high water mark (words): "); \
    Serial.println(uxTaskGetStackHighWaterMark(/* TaskHandle_t xTask = */ NULL))
#else
#define PRINT_STACK_USAGE (void) 0
#endif // PRINT_STACK_DIAGNOSTICS

// Define whether you want to compile the code to WiFi-enable this project, which takes more memory and power
#define WIFI_ENABLED 1

#endif // __FLAGS_H__