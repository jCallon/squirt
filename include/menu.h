#ifndef __MENU_H__
#define __MENU_H__

#include <stdint.h>
#include <stddef.h>

// Include Arduino-dependant I2C LED API
#include <LiquidCrystal_I2C.h>
// Include FreeRTOS common header
#include "freertos/FreeRTOS.h"
// Include FreeRTOS task API
#include "freertos/task.h"
// Include FreeRTOS task handling API
#include "freeRTOS/semphr.h"

// Define what pins are mapped to what peripherals
// See button.h for more
//#define PIN_I2C_DISPLAY_GND GND
//#define PIN_I2C_DISPLAY_VCC VIN
#define PIN_I2C_DISPLAY_SDA ((gpio_num_t) GPIO_NUM_19)
#define PIN_I2C_DISPLAY_SCL ((gpio_num_t) GPIO_NUM_18)

// Define display constants
// Define I2C address for display, see your manufacturer notes to figure out yours
#define DISPLAY_I2C_ADDR 0x27
// Define the number of rows in the LCD display (the number of lines of characters)
#define NUM_DISPLAY_LINES 4
#if NUM_DISPLAY_LINES < 1
#error "Menu code requires at least one line on the display"
#endif
// Define the number of columns in the LCD display (the number of characters in each line)
#define NUM_DISPLAY_CHARS_PER_LINE 20
#if NUM_DISPLAY_CHARS_PER_LINE < 2
#error "Menu code requires at least 2 characters per line on the display"
#endif

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
// Add a new menu_input to the back of the menu input queue
void add_to_menu_input_queue(
    MENU_INPUT_t menu_input,
    bool from_isr);
// Get the main display for the device
LiquidCrystal_I2C *get_display();
// Get the handle of the task that reads the menu input queue
TaskHandle_t get_read_menu_input_queue_task_handle();

// A line within a Menu
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
        // Store current screen buffer (it will be transmitted over WiFi or Bluetooth)
        char display_buffer[sizeof('\0') + (NUM_DISPLAY_LINES * (NUM_DISPLAY_CHARS_PER_LINE + sizeof('\n')))] = { 0 };
};

#endif // __MENU_H__