#ifndef __CONTEXT_H__
#define __CONTEXT_H__

// Include Arduino APIs
#include <Arduino.h>
// Include Arduino-like servo motor API
#include <ESP32Servo.h>
// Include FreeRTOS common header
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task API
#include "freertos/task.h"
// Include FreeRTOS task handling API
#include "freeRTOS/semphr.h"

// Define what pins are mapped to what peripherals
//#define PIN_SERVO_NEG GND
//#define PIN_SERVO_POS VIN
#define PIN_SERVO_OUT ((gpio_num_t) GPIO_NUM_25)

// Define a task for if you want to rotate the servo
void task_rotate_servo(Servo *servo);

// The overall state the menu display and the sensors operate on
class Context
{
    public:
        // Constructor
        Context(
            StaticSemaphore_t *mutex_buffer, 
            int pin_servo_out);
        // Poll the humidity sensor, update context with its reading,
        // the time of its reading, and when the next reading should be
        bool check_humidity();
        // Send signals to the servo motor to move
        bool spray();

        // Menu functions
        // TODO: Is there a better way to do this? Arguments? Lambdas?

        // Add one to percent_desired_humidity,
        // return if control should go back to Menu
        bool add_percent_desired_humidity();
        // Add one to minute_humidity_check_freq, update time_next_humidity_check,
        // return if control should go back to Menu
        bool add_minute_humidity_check_freq();
        // Subtract one from the percent desired humidity,
        // return if control should go back to Menu
        bool subtract_percent_desired_humidity();
        // Subtract one from minute_humidity_check_freq, update time_next_humidity_check,
        // return if control should go back to Menu
        bool subtract_minute_humidity_check_freq();
        // Convert percent_desired_humidity into a human-readable string
        String str_percent_desired_humidity();
        // Convert minute_humidity_check_freq into a human-readable string
        String str_minute_humidity_check_freq();
        // Convert time_last_humidity_check into a human-readable string
        String str_time_last_humidity_check();
        // Convert time_next_humidity_check into a human-readable string
        String str_time_next_humidity_check();
    private:
        // A mutex to keep updating all members of this class thread-safe
        SemaphoreHandle_t mutex_handle;
        // A handle to the servo motor
        Servo servo;
        // A handle to a task that can be used to rotate the servo motor
        TaskHandle_t rotate_servo_task_handle;
        // When the humidity sensor was last checked, what its reaing was
        uint8_t percent_current_humidity;
        // When the humidity sensor is next checked, what to make the humidty at or above
        uint8_t percent_desired_humidity;
        // How often to check the current humidity, in minutes
        uint32_t minute_humidity_check_freq;
        // The time when the humidity was last checked
        time_t time_last_humidity_check;
        // The time when the humidity should next be checked
        time_t time_next_humidity_check;
};

#endif // __CONTEXT_H__