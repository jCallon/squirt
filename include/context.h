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

// Define what pins are mapped to what peripherals
//#define PIN_SERVO_NEG GND
//#define PIN_SERVO_POS VIN
#define PIN_SERVO_OUT ((gpio_num_t) GPIO_NUM_33)
//#define PIN_SOIL_MOISTURE_SENSOR_NEG GND
//#define PIN_SOIL_MOISTURE_SENSOR_POS VIN
#define PIN_SOIL_MOISTURE_SENSOR_IN ((gpio_num_t) GPIO_NUM_35)

// The overall state the menu display and the sensors operate on
class Context
{
    public:
        // Constructor
        Context(
            StaticSemaphore_t *arg_mutex_buffer, 
            int arg_pin_servo_out,
            gpio_num_t arg_pin_soil_moisture_sensor_in);
        // Get whether we are overdue for a humidity check
        bool is_humidity_check_overdue();
        // Get whether the current humidity, at the time of the last check_humidity(), is below desired_humidity
        bool Context::is_current_humidity_below_desired();
        // Poll the humidity sensor, update context with its reading,
        // the time of its reading, and when the next reading should be
        bool check_humidity();
        // Send signals to the servo motor to move
        bool spray(
            bool in_isr,
            bool is_blocking);

        // Menu functions
        // TODO: Is there a better way to do this? Arguments? Lambdas?

        // Add one to minute_humidity_check_freq, update time_next_humidity_check,
        // return if control should go back to Menu
        bool add_minute_humidity_check_freq();
        // Subtract one from the desired humidity,
        // return if control should go back to Menu
        bool subtract_desired_humidity();
        // Subtract one from minute_humidity_check_freq, update time_next_humidity_check,
        // return if control should go back to Menu
        bool subtract_minute_humidity_check_freq();
        // Convert desired_humidity into a human-readable string
        String str_desired_humidity();
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
        // The analog-supporting GPIO pin that reads from the soil moisture sensor
        gpio_num_t pin_soil_moisture_sensor_in;
        // A handle to a task that uses rotate_servo_task_handle to get to desired_humidity
        TaskHandle_t water_task_handle;
        // When the humidity sensor was last checked, what its reading was
        uint16_t current_humidity;
        // When the humidity sensor is next checked, what to make the humidity at or above
        uint16_t desired_humidity;
        // How often to check the current humidity, in minutes
        uint32_t minute_humidity_check_freq;
        // The time when the humidity was last checked
        time_t time_last_humidity_check;
        // The time when the humidity should next be checked
        time_t time_next_humidity_check;
};

// Define a task for if you want to rotate the servo
void task_rotate_servo(Servo *servo);

// Define a task for if you want to reach a desired humidity
void task_water(Context *context);

#endif // __CONTEXT_H__