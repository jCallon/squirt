#include <Arduino.h>

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

// Include FreeRTOS OS API
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task handling API
#include "freeRTOS/task.h"
// Include ESP32 logging API
#include "esp_log.h"
// TODO: comment
#include "driver/gpio.h"
// TODO: comment
#include <LiquidCrystal_I2C.h>

// Create macro for converting milliseconds to clock ticks
#define MS_TO_CLOCK_TICK(ms) ms / portTICK_PERIOD_MS

// Define what pins are mapped to what peripherals
//#define PIN_I2C_DISPLAY_GND GND
//#define PIN_I2C_DISPLAY_VCC VIN
#define PIN_I2C_DISPLAY_SDA ((gpio_num_t) GPIO_NUM_25)
#define PIN_I2C_DISPLAY_SCL ((gpio_num_t) GPIO_NUM_33)
// Each button is connected to GND, in a pull-up resistor fashion (LOW = 0 = pressed, HIGH = 1 = not pressed)
// These defines aren't the state of a button, simply:
// - The button at pin 34 will be used by a user if they want to go up in a menu
// - The button at pin 35 will be used by a user if they want to confirm an option in a menu
// - The button at pin 32 will be used by a user if they want to go down in a menu
// Maybe some renaming is warranted.
#define PIN_BUTTON_UP_IN ((gpio_num_t) GPIO_NUM_34)
//#define PIN_BUTTON_UP_OUT GND
#define PIN_BUTTON_CONFIRM_IN ((gpio_num_t) GPIO_NUM_35)
//#define PIN_BUTTON_CONFIRM_OUT GND
#define PIN_BUTTON_DOWN_IN ((gpio_num_t) GPIO_NUM_32)
//#define PIN_BUTTON_DOWN_OUT GND

// Initialize peripherals
const gpio_num_t button_in_pins[] = {
    PIN_BUTTON_UP_IN,
    PIN_BUTTON_CONFIRM_IN,
    PIN_BUTTON_DOWN_IN
};
LiquidCrystal_I2C display(
    /* uint8_t lcd_Addr = */ 0x27, // << This depends on your display, see your manufaturer notes!
    /* uint8_t lcd_cols = */ 20,
    /* uint8_t lcd_rows = */ 4
);

// Create link so C can find app_main function from C++ file
// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/cplusplus.html
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
    display.print(/* const char *c = */"Hello world!");

    // Set up 3 GPIO buttons
    // While these buttons are pressed, they will be LOW(0), otherwise they stay HIGH(1)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html
    // https://esp32tutorials.com/esp32-push-button-esp-idf-digital-input/
    // https://esp32io.com/tutorials/esp32-button
    for(size_t i = 0; i < (sizeof(button_in_pins) / sizeof(*button_in_pins)); ++i)
    {
        // Clean pre-existing state of this pin
        ESP_ERROR_CHECK(gpio_reset_pin(/* gpio_num_t gpio_num = */ button_in_pins[i]));
        // Read input from this pin
        ESP_ERROR_CHECK(gpio_set_direction(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_mode_t mode = */ GPIO_MODE_INPUT));
        // Use internal pull-up/pull-down resistors instead of external
        ESP_ERROR_CHECK(gpio_set_pull_mode(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_pull_mode_t pull = */ GPIO_PULLUP_ONLY));
#if 0
        // Arduino framework no likey
        ESP_ERROR_CHECK_WITHOUT_ABORT(gpio_dump_io_configuration(
            /* FILE *out_stream = */ stdout,
            /* uint64_t io_bit_mask = */ (1UL << button_in_pins[i])));
#endif

        // TODO: hook up to interrupts to button presses
        // esp_err_t gpio_intr_enable(gpio_num_t gpio_num)
        // esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type)
        // esp_err_t gpio_isr_register(void (*fn)(void*), void *arg, int intr_alloc_flags, gpio_isr_handle_t *handle)
        // esp_err_t gpio_install_isr_service(int intr_alloc_flags)
        // ...
    }
}

void loop()
{
    // Forever...
    // https://www.arduino.cc/reference/en/language/functions/communication/serial
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    // Print "Hello world!" to the serial console
    Serial.println("Hello world!");

    // Show button push status
    // NOTE: they may not be accurate until a few seconds in and they are pushed at least once
    for(size_t i = 0; i < (sizeof(button_in_pins) / sizeof(*button_in_pins)); ++i)
    {
        Serial.print("GPIO pin ");
        Serial.print(button_in_pins[i], DEC);
        Serial.print(": ");
        Serial.print(gpio_get_level(/* gpio_num_t gpio_num = */ button_in_pins[i]), DEC);
        Serial.println("");
    }

    // Yield task for 5 seconds to avoid triggering the watchdog
    vTaskDelay(/*const TickType_t xTicksToDelay = */MS_TO_CLOCK_TICK(2000));
}