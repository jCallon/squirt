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

// Create macro for converting milliseconds to clock ticks
#define MS_TO_CLOCK_TICK(ms) ms / portTICK_PERIOD_MS

// Define what pins are mapped to what buttons
// Each is connected to GND, in a pull-up resistor fashion (LOW = 0 = pressed, HIGH = 1 = not pressed)
// These defines aren't the state of a button, simply:
// - The button at pin 21 will be used by a user if they want to go up in a menu
// - The button at pin 19 will be used by a user if they want to confirm an option in a menu
// - The button at pin 18 will be used by a user if they want to go down in a menu
// Maybe some renaming is warranted.
#define BUTTON_PIN_UP GPIO_NUM_21
#define BUTTON_PIN_CONFIRM GPIO_NUM_19
#define BUTTON_PIN_DOWN GPIO_NUM_18

// NEXT: Print "Hello world" on a I2C display

void app_main()
{
    // Get name of this task
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/search.html?q=pcTaskGetName
    char *task_name = pcTaskGetName(/*TaskHandle_t xTaskToQuery = */ NULL);

    // Set up 3 GPIO buttons
    // While these buttons are pressed, they will be LOW(0), otherwise they stay HIGH(1)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html
    // https://esp32tutorials.com/esp32-push-button-esp-idf-digital-input/
    // https://esp32io.com/tutorials/esp32-button
    gpio_num_t button_in_pins[] = {BUTTON_PIN_UP, BUTTON_PIN_CONFIRM, BUTTON_PIN_DOWN};
    for(size_t i = 0; i < (sizeof(button_in_pins) / sizeof(*button_in_pins)); ++i)
    {
        // Clean pre-existing state of this pin
        if (ESP_OK != gpio_reset_pin(/* gpio_num_t gpio_num = */ button_in_pins[i]))
        {
            // Print message to error serial console
            ESP_LOGE(
                /* const char *tag = */ task_name,
                /* const char *format = */ "Failed to reset GPIO pin %d",
                /* __VA_ARGS__ */ button_in_pins[i]
            );
        }
        // Read input from this pin
        if (ESP_OK != gpio_set_direction(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_mode_t mode = */ GPIO_MODE_INPUT))
        {
            // Print message to error serial console
            ESP_LOGE(
                /* const char *tag = */ task_name,
                /* const char *format = */ "Failed to set direction of GPIO pin %d",
                /* __VA_ARGS__ */ button_in_pins[i]
            );
        }
        // Use internal pull-up/pull-down resistors instead of external
        if (ESP_OK != gpio_set_pull_mode(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_pull_mode_t pull = */ GPIO_PULLUP_ONLY))
        {
            // Print message to error serial console
            ESP_LOGE(
                /* const char *tag = */ task_name,
                /* const char *format = */ "Failed to set GPIO pin %d pull mode to PULLUP",
                /* __VA_ARGS__ */ button_in_pins[i]
            );
        }
        // Verify settings
        if(ESP_OK != gpio_dump_io_configuration(
            /* FILE *out_stream = */ stdout,
            /* uint64_t io_bit_mask = */ (1UL << button_in_pins[i])))
        {
            // Print message to error serial console
            ESP_LOGE(
                /* const char *tag = */ task_name,
                /* const char *format = */ "Failed to dump settings fot GPIO pin %d to stdout",
                /* __VA_ARGS__ */ button_in_pins[i]
            );
        }
        // TODO: hook up to interrupts to button presses
        // esp_err_t gpio_intr_enable(gpio_num_t gpio_num)
        // esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_int_type_t intr_type)
        // esp_err_t gpio_isr_register(void (*fn)(void*), void *arg, int intr_alloc_flags, gpio_isr_handle_t *handle)
        // esp_err_t gpio_install_isr_service(int intr_alloc_flags)
        // ...
    }

    // Forever...
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while(1)
    {
        // Print "Hello world!" to the information serial console
        ESP_LOGI(
            /*const char *tag = */ task_name,
            /*const char *format = */ "Hello world!"
        );

        // Show button push status
        for(size_t i = 0; i < (sizeof(button_in_pins) / sizeof(*button_in_pins)); ++i)
        {
            ESP_LOGI(
                /* const char *tag = */ task_name,
                /* const char *format = */ "GPIO pin %d: %d",
                /* __VA_ARGS__ */ button_in_pins[i],
                /* __VA_ARGS__ */ gpio_get_level(/* gpio_num_t gpio_num = */ button_in_pins[i])
            );
        }

        // Yield task for 5 seconds to avoid triggering the watchdog
        vTaskDelay(/*const TickType_t xTicksToDelay = */MS_TO_CLOCK_TICK(5000));
    }
}