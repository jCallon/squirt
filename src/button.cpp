//TODO: comment
#include "button.h"
// TODO: comment
#include <Arduino.h>

// https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/gpio.html
// https://esp32tutorials.com/esp32-push-button-esp-idf-digital-input/
// https://esp32io.com/tutorials/esp32-button

// Instantiate instance of buttons
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

// Declare static functions
static void task_read_button_press();
static void IRAM_ATTR intr_write_button_press(gpio_num_t gpio_pin);

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

    // TODO: Look into static memory instead of heap memory task creation, better? Better arguments?
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

// TODO: Check stack/heap size.
static void task_read_button_press()
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
    // TODO: This code is gross and error prone
    Button *button = NULL;
    switch(gpio_pin)
    {
        case PIN_BUTTON_UP_IN:
            button = &buttons[0];
            break;
        case PIN_BUTTON_CONFIRM_IN:
            button = &buttons[1];
            break;
        case PIN_BUTTON_DOWN_IN:
            button = &buttons[2];
            break;
        default:
            return;
    }
    if(false == button->is_button_press())
    {
        return;
    }

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
    is_pressed = false;
    is_pull_up = arg_is_pull_up;
    ms_debounce = arg_ms_debounce;
    pin_in = arg_pin_in;
    ms_next_valid_edge = 0;
    // Mutex type semaphores cannot be used from within intrrupt service routines.
    // https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/freertos_idf.html
    // TODO: look into 'direct to task notification'
    //mutex_handle = xSemaphoreCreateBinaryStatic(&mutex_buffer);
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
    // This button press logic is flawed, but good enough for now.
    // User IO is not the main concern of this system, or FreeRTOS generally.
    // Ex:
    // HI _____                   _____
    // LO      \/\/\/\_____/\/\/\/
    // 0) assume all buttons start unpressed and in HIGH
    // 1) set the button as pressed once the first static happens
    // 2) ignore static for next X ms
    // 3) set button as unpressed once the first static happens
    // 4) ignore static for the next X ms
    // 5) repeat starting from 2

    // Get the current time in milliseconds
    unsigned long ms_current_time = millis();

    // Only allow one thread to read/modify this button's members at a time
    // If the semaphore is not available, wait 25 ticks to see if it becomes free
#if 0
    if ((NULL != mutex_handle) &&
        (pdTRUE == xSemaphoreTakeFromISR(
            /* xSemaphore = */ mutex_handle,
            /* xBlockTime = */ TickType_t(25))))
    {
#endif
        // If the current time is at or past the time this edge is no longer considered noise...
        if (ms_current_time >= ms_next_valid_edge)
        {
            // Update when the next edge trasition is no longer considered noise
            ms_next_valid_edge = ms_current_time + ms_debounce;
            is_pressed = !is_pressed;
        }

#if 0
        // Free sempahore so other threads can read/modify this button's members
        xSemaphoreGiveFromISR(mutex_handle);
    }
#endif

    return is_pressed;
}