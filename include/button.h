#ifndef __BUTTON_H__
#define __BUTTON_H__

// TODO: comment
#include "driver/gpio.h"
// Include FreeRTOS task handling API
//#include "freeRTOS/task.h"

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
#define NUM_BUTTONS 3

// Initialize the GPIO button press queue and enable per-pin interrupts
void init_gpio();

class Button
{
    public:
        // Constructor for Button
        Button(bool arg_is_pull_up, uint8_t arg_ms_debounce, gpio_num_t arg_pin_in);
        // Configure GPIO pin pin_in to read input from this button
        void register_pin();
        // Set up this button to have interrupts on either:
        // - is_pull_up == TRUE, on negative edges, when the state of the button transitions from HIGH (1) to LOW (0)
        // - is_pull_up == FALSE, on positive edges, when the state of the button transitions from LOW (0) to HIGH (1)
        // This function will register XXX as its interrupt routine (the function called when an interrupt is received), with pin_in as the argument.
        void register_intr();
        // Determine whether this interrupt is too close to the previous, and is 'noise', or is not, and is a valid button press.
        bool is_button_press();

    private:
        // TODO: comment
        bool is_pressed;
        // Whether the button is pull-up or pull-down.
        // If the button is pull-up, its default state is HIGH (1), and its pressed state is LOW (0).
        // If the button is not pull-up, it is pull-down, and its default state is is LOW (0), and its pressed state is HIGH (1).
        bool is_pull_up;
        // When a digital button is pressed, it may flicker between HIGH (1) and LOW (0) many times before finally settling into its new state.
        // For example, with a pull-up button, when you press it, it might alternate between HIGH (1) and LOW (0) several times before staying on LOW.
        // To not register this flickering 'noise' as repeated button presses, which it's not,
        // do not count edges within ms_debouce milliseconds of each other as new button presses.
        // https://esp32io.com/tutorials/esp32-button-debounce
        uint8_t ms_debounce;
        // The number of the GPIO pin listening for input from the button.
        gpio_num_t pin_in;
        // At what time, in milliseconds since the processor started running,
        // an interrupt for this edge can be considered a valid button press again instead of just noise.
        unsigned long ms_next_valid_edge;
        // TODO: look into mutexes
        // A mutual exlusion to apply when only onw thread should be able to access something at a time.
        //SemaphoreHandle_t mutex_handle;
        // A statically allocated buffer for the semaphore to live in
        //StaticSemaphore_t mutex_buffer;
};

#endif // __BUTTON_H__