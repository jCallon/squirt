#ifndef __CONTEXT_H__
#define __CONTEXT_H__

// Include Arduino-like servo motor API
#include <ESP32Servo.h>
// Include FreeRTOS common header
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task API
#include "freertos/task.h"
// Include FreeRTOS task handling API
#include "freeRTOS/semphr.h"

// Include custom Menu class implementation
#include "menu.h"
// Include custom storage API
#include "storage.h"

// Define what pins are mapped to what peripherals
//#define PIN_SERVO_NEG GND
//#define PIN_SERVO_POS VIN
#define PIN_SERVO_OUT ((gpio_num_t) GPIO_NUM_33)
//#define PIN_SOIL_MOISTURE_SENSOR_NEG GND
//#define PIN_SOIL_MOISTURE_SENSOR_POS VIN
#define PIN_SOIL_MOISTURE_SENSOR_IN ((gpio_num_t) GPIO_NUM_35)

// Create macros to avoid erroneous/annoying copy/paste
// Take the Context sempahore to prevent miscellaneous reads and writes from other threads
#define CONTEXT_LOCK(RET_VAL) \
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 100)) return RET_VAL;
// Return mutex so other threads can read and write to context members again
#define CONTEXT_UNLOCK() xSemaphoreGive(/* xSemaphore = */ mutex_handle);

#define CONTEXT_NVS_KEY_MINUTE_SOIL_MOISTURE_CHECK_FREQ "read_freq"
#define CONTEXT_NVS_KEY_DESIRED_SOIL_MOISTURE "desired_moisture"

// The overall state the menu display and the sensors operate on
class Context
{
    public:
        // Constructor
        Context(
            StaticSemaphore_t *arg_mutex_buffer, 
            int arg_pin_servo_out,
            gpio_num_t arg_pin_soil_moisture_sensor_in,
            char *arg_nvs_namespace);

        // Get whether we are overdue for a soil moisture check
        bool is_soil_moisture_check_overdue();
        // Get whether the current soil moisture, from the last check_soil_moisture(), is below our desired soil moisture
        bool is_current_soil_moisture_below_desired();

        // Poll the soil moisture sensor, update the context with its reading, the time it was taken, and when it should next be taken
        MENU_CONTROL check_soil_moisture(bool update_next_soil_moisture_check);

        // Trigger water_task_handle, telling the servo to move many times until our desired soil moisture is reached
        MENU_CONTROL water();
        // Trigger rotate_servo_task_handle, telling the servo to move once
        MENU_CONTROL spray(
            bool in_isr,
            bool is_blocking);

        // Menu functions //
        // TODO: Is there a better way to do this? Arguments? Lambdas?

        // Set desired_soil_moisture to current_soil_moisture
        MENU_CONTROL set_desired_soil_moisture_to_current();
        // Add num_minutes to minute_moisture_check_freq, update time_next_moisture_check
        MENU_CONTROL add_minute_soil_moisture_check_freq(int num_minutes);

        // Get current_soil_moisture as a human-readable formatted string
        String str_current_soil_moisture();
        // Get desired_soil_moisture as a human-readable formatted string
        String str_desired_soil_moisture();
        // Get minute_soil_moisture_check_freq as a human-readable formatted string
        String str_minute_soil_moisture_check_freq();
        // Get time_last_soil_moisture_check as a human-readable formatted string
        String str_time_last_soil_moisture_check();
        // Get time_next_soil_moisture_check as a human-readable formatted string
        String str_time_next_soil_moisture_check();

    private:
        // A mutex to keep updating all members of this class thread-safe
        // Easier, but slower to have one mutex for all members than one for each
        SemaphoreHandle_t mutex_handle;

        // A handle to a task that periodically checks the soil moisture, adds water when below our desired moisture, see: task_water
        TaskHandle_t water_task_handle;
        // A handle to a task that can be used to rotate the servo motor, see: task_rotate_servo
        TaskHandle_t rotate_servo_task_handle;

        // A handle to the servo motor
        Servo servo;
        // The ADC (Analog to Digital Converter) supporting GPIO pin that reads the soil moisture sensor output
        gpio_num_t pin_soil_moisture_sensor_in;

        // The namespace within NVS this Context maps to
        char *nvs_namespace;
        // The handle to access nvs_namespace within NVS
        nvs_handle_t nvs_handle;

        // When the soil moisture sensor was last checked, what its reading was
        uint16_t current_soil_moisture;
        // When next watering, what to make the soil moisture at or above
        uint16_t desired_soil_moisture;
        // How often to check the soil moisture, in minutes
        uint32_t minute_soil_moisture_check_freq;
        // The time when the soil moisture was last checked
        time_t time_last_soil_moisture_check;
        // The time when the soil moisture should be next checked
        time_t time_next_soil_moisture_check;
};

// Define a task for rotating a servo to neutral, an angle, and back
void task_rotate_servo(Servo *servo);
// Define a task for periodically checking, then adding water if soil moisture is below desired
void task_water(Context *context);

#endif // __CONTEXT_H__