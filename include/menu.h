#ifndef __MENU_H__
#define __MENU_H__

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

class MenuLine
{
    public:
        // Constructor
        MenuLine(
            char *str_display,
            //void (*arg_to_str)(),
            void (*arg_func_on_up)(),
            void (*arg_func_on_confirm)(),
            void (*arg_func_on_down)());
        // Call func_on_* based on what menu input was received was
        void react_to_button_press(MENU_INPUT_t input);
        // Get the string this menu line should currently be displaying as
        // If the function returns true, that means the string has changed since it was last requested,
        // and the display should be updated, if this line is on the screen
        bool get_str(char **arg_str);

    private:
        // Whether str_display changes as a result of reacting to a button press, and menu should update
        bool str_display_has_changed;
        // The string this line is currently displayed as
        char *str_display;
        // The function that should be used to used to convert this menu line to a string.
        //void (*func_to_str)();
        // When a user clicks confirm on this menu line while modifying it, the function it will call.
        void (*func_on_confirm)();
        // When a user clicks up on this menu line while modifying it, the function it will call.
        void (*func_on_up)();
        // When a user clicks down on this menu line while modifying it, the function it will call.
        void (*func_on_down)();
        // Sometimes a menu option wil take more than once press of confirm, such as when changing a string.
        // Use this counter to keep track of how far you are.
        //bool num_confirm;
};

class Menu
{
    public:
        Menu(MenuLine *arg_menu_line);
    private:
        // All of the lines available in the menu
        MenuLine *menu_lines;
        // The index of the menu line the user is currently hovering over or selecting
        size_t index_menu_item_hover;
        // Whether a menu item is currently selected, so menu inputs should be forwarded to the MenuLine's handlers instead of navigating Menu
        bool is_menu_item_selected;
};

#endif // __MENU_H__