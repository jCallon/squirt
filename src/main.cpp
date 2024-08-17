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
// Include ESP32 logging API
#include "esp_log.h"
// TODO: comment
#include "driver/gpio.h"
// TODO: comment
#include <LiquidCrystal_I2C.h>

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
// Each button is connected to GND, in a pull-up resistor fashion (LOW = 0 = pressed, HIGH = 1 = not pressed)
// These defines aren't the state of a button, simply:
// - The button at pin 34 will be used by a user if they want to go up in a menu
// - The button at pin 35 will be used by a user if they want to confirm an option in a menu
// - The button at pin 32 will be used by a user if they want to go down in a menu
// Maybe some renaming is warranted.
#define PIN_BUTTON_UP_IN ((gpio_num_t) GPIO_NUM_25)
//#define PIN_BUTTON_UP_OUT GND
#define PIN_BUTTON_CONFIRM_IN ((gpio_num_t) GPIO_NUM_33)
//#define PIN_BUTTON_CONFIRM_OUT GND
#define PIN_BUTTON_DOWN_IN ((gpio_num_t) GPIO_NUM_32)
//#define PIN_BUTTON_DOWN_OUT GND

// ================================= //
// Initialize useful data structures //
// ================================= //

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
static QueueHandle_t gpio_event_queue = NULL;

// ============================================ //
// Declare tasks and interrupts for peripherals //
// ============================================ //

// Define event (interrupt) for a GPIO button press
// This ISR handler will be called from an ISR.
// So there is a stack size limit (configurable as "ISR stack size" in menuconfig).
// This limit is smaller compared to a global GPIO interrupt handler due to the additional level of indirection.
// TODO: What does IRAM_ATTR do?
//       Check stack/heap size.
// https://github.com/espressif/esp-idf/blob/v5.3/examples/peripherals/gpio/generic_gpio/main/gpio_example_main.c
static void IRAM_ATTR intr_write_button_press(gpio_num_t gpio_pin)
{
    // Write GPIO pin number to queue holding all button presses
    // Remember, non-ISR queue access is not safe from within interrupts!
    xQueueSendFromISR(
        // The handle to the queue on which the item is to be posted.
        /* QueueHandle_t xQueue = */ gpio_event_queue,
        // A pointer to the item that is to be placed on the queue.
        // The size of the items the queue will hold was defined when the queue was created,
        // so this many bytes will be copied from pvItemToQueue into the queue storage area.
        /* const void *const pvItemToQueue = */ &gpio_pin,
        // xQueueGenericSendFromISR() will set *pxHigherPriorityTaskWoken to pdTRUE if sending to the queue caused a task to unblock,
        // and the unblocked task has a priority higher than the currently running task.
        // If xQueueGenericSendFromISR() sets this value to pdTRUE then a context switch should be requested before the interrupt is exited.
        /* BaseType_t *const pxHigherPriorityTaskWoken = */ NULL
    );
}

// TODO: Check stack/heap size.
static void task_read_button_press()
{
    // Get GPIO pin number from queue holding all button presses
    gpio_num_t gpio_pin = GPIO_NUM_0;
    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while (1)
    {
        if(xQueueReceive(
            // The handle to the queue from which the item is to be received.
            /* QueueHandle_t xQueue = */ gpio_event_queue,
            // Pointer to the buffer into which the received item will be copied.
            /* void *pvBuffer = */ &gpio_pin,
            // The maximum amount of time the task should block waiting for an item to receive should the queue be empty at the time of the call.
            // xQueueReceive() will return immediately if xTicksToWait is zero and the queue is empty.
            // The time is defined in tick periods so the constant portTICK_PERIOD_MS should be used to convert to real time if this is required.
            /* TickType_t xTicksToWait */ portMAX_DELAY))
        {
            switch(gpio_pin)
            {
                case PIN_BUTTON_UP_IN:
                    Serial.println("Button UP pressed");
                    break;
                case PIN_BUTTON_CONFIRM_IN: 
                    Serial.println("Button CONFIRM pressed");
                    break;
                case PIN_BUTTON_DOWN_IN:
                    Serial.println("Button DOWN pressed");
                    break;
                default:
                    Serial.println("Button from unrecognized GPIO pin triggered intr_button_press");
                    break;
            }
        }
    }
}

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

    // Create 10 element long queue for GPIO-pin interrupts that simply stores the pin that causes the interrupt
    // TODO: this would probably be ok to make as a static queue (no dynamic memory needed)
    gpio_event_queue = xQueueCreate(
        /* uxQueueLength = */ 10,
        /* uxItemSize = */ sizeof(gpio_num_t));

    // Allow per-GPIO-pin interrupts
    // TODO: Should I remove the interrupt config between device reprogramming?
    //       What are GPIO pads?
    //       Look into light and deep sleep mode to reduce power consumption.
    // - ESP_INTR_FLAG_LEVEL1: Interrupt allocation flags.
    //   These flags can be used to specify which interrupt qualities the code calling esp_intr_alloc* needs.
    //   Accept a Level 1 interrupt vector (lowest priority)
    // - ESP_INTR_FLAG_EDGE: Edge-triggered interrupt. 
    // - ESP_INTR_FLAG_LOWMED: Low and medium prio interrupts. These can be handled in C.
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/intr_alloc.html
    ESP_ERROR_CHECK(gpio_install_isr_service(/* int intr_alloc_flags = */ ESP_INTR_FLAG_LEVEL1 | ESP_INTR_FLAG_EDGE | ESP_INTR_FLAG_LOWMED));

    // Set up GPIO buttons
    // While these buttons are pressed, they will be LOW(0), otherwise they stay HIGH(1)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/log.html
    // https://esp32tutorials.com/esp32-push-button-esp-idf-digital-input/
    // https://esp32io.com/tutorials/esp32-button
    for(size_t i = 0; i < (sizeof(button_in_pins) / sizeof(*button_in_pins)); ++i)
    {
        // Clean pre-existing state of this pin
        ESP_ERROR_CHECK(gpio_reset_pin(/* gpio_num_t gpio_num = */ button_in_pins[i]));

        // Set this GPIO pin to read input
        ESP_ERROR_CHECK(gpio_set_direction(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_mode_t mode = */ GPIO_MODE_INPUT));

        // Use this GPIO pin's internal pull-up/pull-down resistors instead of external resistors
        // NOTE: GPIO 34-39 do not have both integrated pull-up and pull-down resistors
        ESP_ERROR_CHECK(gpio_set_pull_mode(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_pull_mode_t pull = */ GPIO_PULLUP_ONLY));

        // Set GPIO interrupt trigger type to falling edge (when the signal goes from 1 to 0)
        ESP_ERROR_CHECK(gpio_set_intr_type(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_int_type_t intr_type = */ GPIO_INTR_NEGEDGE));

        // Register interrupt handler for this GPIO pin specifically, it will call intr_write_button_press on falling edge
        ESP_ERROR_CHECK(gpio_isr_handler_add(
            /* gpio_num_t gpio_num = */ button_in_pins[i],
            /* gpio_isr_t isr_handler = */ (gpio_isr_t) intr_write_button_press,
            /* void *args = */ (void *) button_in_pins[i]));

        // Enable GPIO module interrupt for this GPIO pin
        ESP_ERROR_CHECK(gpio_intr_enable(/* gpio_num_t gpio_num = */ button_in_pins[i]));
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
        /* const configSTACK_DEPTH_TYPE usStackDepth = */ 2048,
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