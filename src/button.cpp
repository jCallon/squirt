//TODO: comment
#include "button.h"
// TODO: comment
#include <Arduino.h>
// Include FreeRTOS OS API
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task handling API
//#include "freeRTOS/task.h"

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
// https://esp32tutorials.com/esp32-push-button-esp-idf-digital-input/
// https://esp32io.com/tutorials/esp32-button

// TODO: In the future, 'button' events may not be from buttons, but also from WiFi and Bluetooth.
//       This is a poor way to handle the button queue, but it works for now.
static QueueHandle_t gpio_event_queue = NULL;
void init_gpio()
{
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
}

// TODO: Check stack/heap size.
void task_read_button_press()
{
    // Get GPIO pin number from queue holding all button presses
    gpio_num_t gpio_pin = GPIO_NUM_0;

    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while (1)
    {
        // Wait for an item to enter the button press queue
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
            // Depending on the button in the queue, print a different message
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

Button::Button(
    bool arg_is_pull_up,
    uint8_t arg_ms_debounce,
    gpio_num_t arg_pin_in)
{
    is_pull_up = arg_is_pull_up;
    ms_debounce = arg_ms_debounce;
    pin_in = arg_pin_in;
    ms_next_valid_edge = 0;
    // Mutex type semaphores cannot be used from within intrrupt service routines.
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos_idf.html
    //mutex = xSemaphoreCreateMutexStatic(&mutex_buffer);
}

void Button::register_pin()
{
    // Clean pre-existing state of this pin
    ESP_ERROR_CHECK(gpio_reset_pin(/* gpio_num_t gpio_num = */ pin_in));

    // Set this GPIO pin to read input
    ESP_ERROR_CHECK(gpio_set_direction(
        /* gpio_num_t gpio_num = */ pin_in,
        /* gpio_mode_t mode = */ GPIO_MODE_INPUT));

    // Use this GPIO pin's internal pull-up/pull-down resistors instead of external resistors
    // NOTE: GPIO 34-39 do not have both integrated pull-up and pull-down resistors
    ESP_ERROR_CHECK(gpio_set_pull_mode(
        /* gpio_num_t gpio_num = */ pin_in,
        /* gpio_pull_mode_t pull = */ is_pull_up ? GPIO_PULLUP_ONLY : GPIO_PULLDOWN_ONLY));
}

void Button::register_intr()
{
    // Set GPIO interrupt trigger type to the correct edge for whether this is a pull-up or pull-down resistor button
    ESP_ERROR_CHECK(gpio_set_intr_type(
        /* gpio_num_t gpio_num = */ pin_in,
        /* gpio_int_type_t intr_type = */ is_pull_up ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE));

    // Register interrupt handler for this GPIO pin specifically, it will call intr_write_button_press on falling/rising edge
    ESP_ERROR_CHECK(gpio_isr_handler_add(
        /* gpio_num_t gpio_num = */ pin_in,
        /* gpio_isr_t isr_handler = */ (gpio_isr_t) intr_write_button_press,
        /* void *args = */ (void *) pin_in));

    // Enable GPIO module interrupt for this GPIO pin
    ESP_ERROR_CHECK(gpio_intr_enable(/* gpio_num_t gpio_num = */ pin_in));
}

bool Button::is_button_press()
{
    // Store result of check
    bool status = false;
    // Get the current time in milliseconds
    unsigned long ms_current_time = millis();

    // Only allow one thread to read/modify this button's members at a time
    //mutex.lock();

    // If the current time is at or past the time this edge is no longer considered noise...
    if (ms_current_time >= ms_next_valid_edge)
    {
        // Update when the next edge trasition is no longer considered noise
        ms_next_valid_edge = ms_current_time + ms_debounce;
        status = true;
    }

    // Free lock so other threads can read/modify this button's members
    //mutex.unlock();

    return status;
}