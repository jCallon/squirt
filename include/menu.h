#ifndef __MENU_H__
#define __MENU_H__

#include <stdint.h>
#include <stddef.h>

// Include Arduino APIs
#include <Arduino.h>
// Include FreeRTOS task handling API
#include "freeRTOS/semphr.h"

// Define what pins are mapped to what peripherals
// See button.h for more
//#define PIN_I2C_DISPLAY_GND GND
//#define PIN_I2C_DISPLAY_VCC VIN
#define PIN_I2C_DISPLAY_SDA ((gpio_num_t) GPIO_NUM_27)
#define PIN_I2C_DISPLAY_SCL ((gpio_num_t) GPIO_NUM_26)

enum MENU_INPUT_t : uint8_t
{
    MENU_INPUT_NONE = 0,
    MENU_INPUT_UP,
    MENU_INPUT_CONFIRM,
    MENU_INPUT_DOWN,
    MENU_INPUT_MAX
};

// Initialze menu and its input queue/task
void init_menu();
// From an interrupt-service-routine, add a new menu_input to the back of the menu input queue
void from_isr_add_to_menu_input_queue(MENU_INPUT_t menu_input);

// A line within a menu
class MenuLine
{
    public:
        // Constructor
        MenuLine(
            String arg_str_display,
            String (*arg_func_to_str)(),
            bool (*arg_func_on_up)(),
            bool (*arg_func_on_confirm)(),
            bool (*arg_func_on_down)());
        // Call func_on_* based on what menu input was received
        // Returns true if, after this press, it is giving control back to the menu.
        bool react_to_menu_input(MENU_INPUT_t input);
        // Get the string this menu line should currently be displaying as
        // If the function returns true, that means the string has changed since it was last requested,
        // and the display should be updated, if this line is on the screen
        bool get_str(String **arg_str);

    private:
        // Whether str_display string has been updaed since its underlying data has changed.
        bool is_str_and_data_desynced;
        // The string this line is currently displayed as.
        String str_display;
        // The function that should be used to used to convert this menu line to a string.
        String (*func_to_str)();
        // When a user clicks confirm on this menu line while modifying it, the function it will call.
        // It should return true if, after this press, it is giving control back to the menu.
        bool (*func_on_confirm)();
        // When a user clicks up on this menu line while modifying it, the function it will call.
        // It should return true if, after this press, it is giving control back to the menu.
        bool (*func_on_up)();
        // When a user clicks down on this menu line while modifying it, the function it will call.
        // It should return true if, after this press, it is giving control back to the menu.
        bool (*func_on_down)();
        // Sometimes a menu option wil take more than once press of confirm, such as when changing a string.
        // Use this counter to keep track of how far you are.
        //bool num_confirm;
};

// A menu meant to display on a screen
class Menu
{
    public:
        // Constructor
        Menu(
            MenuLine *arg_menu_lines,
            size_t arg_num_menu_lines);
        // Decide what to do based on what menu input was received
        void react_to_menu_input(MENU_INPUT_t menu_input);
        // Update what is displayed on the I2C LED display
        // NOTE: Right now this is only coded for a 20x04 I2C LED display, and lines over 18 hcaracter will have a bad time
        void update_display();
    private:
        // All of the lines available in the menu
        MenuLine *menu_lines;
        // The number of lines available in the menu
        size_t num_menu_lines;
        // The index of the menu line the user is currently hovering over or selecting
        size_t index_menu_item_hover;
        // Whether a menu item is currently selected, so menu inputs should be forwarded to the MenuLine's handlers instead of navigating Menu
        bool is_menu_item_selected;
};

// The overall state the menu display and the sensors operate on
class Context
{
    public:
        // Constructor
        Context(StaticSemaphore_t *mutex_buffer);
        // TODO: comment
        bool check_humidity();
        // TODO: comment
        bool spray();
        // Menu functions
        // TODO: Is there a better way to do this? Arguments? Lambdas?
        bool add_percent_desired_humidity();
        bool add_minute_humidity_check_freq();
        bool subtract_percent_desired_humidity();
        bool subtract_minute_humidity_check_freq();
        String str_percent_desired_humidity();
        String str_minute_humidity_check_freq();
        String str_time_last_humidity_check();
        String str_time_next_humidity_check();
    private:
        // A mutex to keep updating all members of this class thread-safe.
        SemaphoreHandle_t mutex_handle;
        // When the humidity sensor was last checked, what its reaing was.
        uint8_t percent_current_humidity;
        // When the humidity sensor is next checked, what to make the humidty at or above.
        uint8_t percent_desired_humidity;
        // How often to check the current humidity, in minutes.
        uint32_t minute_humidity_check_freq;
        // The time when the humidity was last checked.
        time_t time_last_humidity_check;
        // The time when the humidity should next be checked.
        time_t time_next_humidity_check;
};

#endif // __MENU_H__