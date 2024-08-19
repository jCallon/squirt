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

// ========================== //
// Include external libraries //
// ========================== //

// TODO: comment
#include <Arduino.h>
// Include FreeRTOS OS API
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task handling API
#include "freeRTOS/task.h"
// TODO: comment
#include "driver/gpio.h"
// TODO: comment
#include <LiquidCrystal_I2C.h>
//TODO: comment
#include "button.h"

// ===================== //
// Declare useful macros //
// ===================== //

// Create macro for converting milliseconds to clock ticks
#define MS_TO_CLOCK_TICK(ms) ms / portTICK_PERIOD_MS

// Define what pins are mapped to what peripherals
//#define PIN_I2C_DISPLAY_GND GND
//#define PIN_I2C_DISPLAY_VCC VIN
#define PIN_I2C_DISPLAY_SDA ((gpio_num_t) GPIO_NUM_27)
#define PIN_I2C_DISPLAY_SCL ((gpio_num_t) GPIO_NUM_26)

// ================================= //
// Initialize useful data structures //
// ================================= //
// Initialize peripherals
Button buttons[NUM_BUTTONS] = {
    { 
        /* bool arg_is_pull_up = */ true,
        /* uint8_t arg_ms_debounce = */ 50,
        /* gpio_num_t arg_pin_in = */ PIN_BUTTON_UP_IN
    },
    { 
        /* bool arg_is_pull_up = */ true,
        /* uint8_t arg_ms_debounce = */ 50,
        /* gpio_num_t arg_pin_in = */ PIN_BUTTON_CONFIRM_IN
    },
    { 
        /* bool arg_is_pull_up = */ true,
        /* uint8_t arg_ms_debounce = */ 50,
        /* gpio_num_t arg_pin_in = */ PIN_BUTTON_DOWN_IN
    }
};
LiquidCrystal_I2C display(
    /* uint8_t lcd_Addr = */ 0x27, // << This depends on your display, see your manufaturer notes!
    /* uint8_t lcd_cols = */ 20,
    /* uint8_t lcd_rows = */ 4
);

// ============================================ //
// Declare tasks and interrupts for peripherals //
// ============================================ //

// =========================== //
// Initialize and start device //
// =========================== //

// Start program
void setup()
{
    // Initialize Arduino serial console
    Serial.begin(/* unsigned long baud = */ 115200);
    // Wait for serial port to connect
    while(!Serial);

    // Initialize LCD display, clear anything on it, turn on the backlight, and print "Hello world!"
    // https://lastminuteengineers.com/esp32-i2c-lcd-tutorial/
    // https://forum.arduino.cc/t/liquidcrystal_i2c-how-to-change-pins/572686/7
    Wire.begin(
        /* int sda = */ PIN_I2C_DISPLAY_SDA,
        /* int sdl = */ PIN_I2C_DISPLAY_SCL
    );
    display.init();
    display.clear();
    display.backlight();
    display.setCursor(
        /* uint8_t col = */ 0,
        /* uint8_t row = */ 0
    );
    display.print(/* const char *c = */ "Hello world!");

    // Initialize the GPIO button press queue and enable per-pin interrupts
    init_gpio();

    // Set up GPIO buttons
    for(size_t i = 0; i < NUM_BUTTONS; ++i)
    {
        buttons[i].register_pin();
        buttons[i].register_intr();
    }

#if 0
// Arduino framework no likey
    ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_dump_io_configuration(
        /* FILE *out_stream = */ stdout,
        /* uint64_t io_bit_mask = */ (1UL << button_in_pins[i])));
#endif

    // TODO: Look into static memory instead of heap memory task creation, better? Better arugments?
    //       Check return value (TaskFunction_t)
    xTaskCreate(
        // Pointer to the task entry function. Tasks must be implemented to never return (i.e. continuous loop).
        /* TaskFunction_t pxTaskCode = */ (TaskFunction_t) task_read_button_press,
        // A descriptive name for the task. This is mainly used to facilitate debugging. Max length defined by configMAX_TASK_NAME_LEN - default is 16.
        /* const char *const pcName = */ "read_gpio_queue",
        // The size of the task stack specified as the NUMBER OF BYTES. Note that this differs from vanilla FreeRTOS.
        /* const configSTACK_DEPT_TYPE usStackDepth = */ 2048,
        // Pointer that will be used as the parameter for the task being created.
        /* void *const pvParameters = */ NULL,
        // The priority at which the task should run.
        // Systems that include MPU support can optionally create tasks in a privileged (system) mode by setting bit portPRIVILEGE_BIT of the priority parameter.
        // For example, to create a privileged task at priority 2 the uxPriority parameter should be set to ( 2 | portPRIVILEGE_BIT ).
        /* UBaseType_t uxPriority = */ 10,
        // Used to pass back a handle by which the created task can be referenced.
        /* TaskHandle_t *const pxCreatedTask = */ NULL);
}

// Don't have any need for a loop that runs forever, because we're using FreeRTOS tasks,
// but this function needs to be defined to make the Arduino framework happy
void loop()
{
}