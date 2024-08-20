// Include Arduino-dependant I2C LED API
#include <LiquidCrystal_I2C.h>
// Include Arduino APIs
#include <Arduino.h>
// Include FreeRTOS queue API
#include "freertos/queue.h"
// Include custom menu class implementation
#include "menu.h"

// ======================= //
// Instantiate useful data //
// ======================= //

// Define a thread and interrupt safe queue to hold menu inputs
QueueHandle_t menu_input_queue_handle;

// Initialize peripheral
LiquidCrystal_I2C display(
    /* uint8_t lcd_Addr = */ 0x27, // << This depends on your display, see your manufaturer notes!
    /* uint8_t lcd_cols = */ 20,
    /* uint8_t lcd_rows = */ 4
);

#if 0
// Define custom character for water emoji
// represented in the display example below as X
const uint8_t water_emoji = {
    0b00100,
    0b00100,
    0b01110,
    0b01110,
    0b11111,
    0b11111,
    0b11111,
    0b01110,
};

// Define custom character for water emoji
// represented in the display example below as X
const uint8_t full_right_arrow_emoji = {
    0b01000,
    0b01100,
    0b01110,
    0b01111,
    0b01110,
    0b01100,
    0b01000,
    0b00000,
};
#endif

// TODO: properly define functions and such for all of these
static MenuLine menu_lines[] = 
{
    {
        /* char *str_display = */ "Goal X: 50%",
        /* void (*arg_func_on_up)() = */ nullptr,
        /* void (*arg_func_on_confirm)() = */ nullptr,
        /* void (*arg_func_on_down)() = */ nullptr
    },
    {
        /* char *str_display = */ "X freq: 1000 min",
        /* void (*arg_func_on_up)() = */ nullptr,
        /* void (*arg_func_on_confirm)() = */ nullptr,
        /* void (*arg_func_on_down)() = */ nullptr
    },
    {
        /* char *str_display = */ "Last X: 08:00 AM",
        /* void (*arg_func_on_up)() = */ nullptr,
        /* void (*arg_func_on_confirm)() = */ nullptr,
        /* void (*arg_func_on_down)() = */ nullptr
    },
    {
        /* char *str_display = */ "Next X: 12:00 PM",
        /* void (*arg_func_on_up)() = */ nullptr,
        /* void (*arg_func_on_confirm)() = */ nullptr,
        /* void (*arg_func_on_down)() = */ nullptr
    },
    {
        /* char *str_display = */ "Check X",
        /* void (*arg_func_on_up)() = */ nullptr,
        /* void (*arg_func_on_confirm)() = */ nullptr,
        /* void (*arg_func_on_down)() = */ nullptr
    },
    {
        /* char *str_display = */ "Trigger spray",
        /* void (*arg_func_on_up)() = */ nullptr,
        /* void (*arg_func_on_confirm)() = */ nullptr,
        /* void (*arg_func_on_down)() = */ nullptr
    }
};

// Create an instance of a menu
// TODO: use a #define or someting better than hard-coding this value
static Menu menu = {
    /* MenuLine *arg_menu_lines = */ menu_lines,
    /* size_t arg_num_menu_lines = */ 6
};

// =================================================== //
// Functions for interacting with the menu input queue //
// =================================================== //

static void task_read_menu_input_queue();

void init_menu()
{
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

    // Set queue_handle_menu_input to point to a queue capable of holding 10 menu inputs
    menu_input_queue_handle = xQueueCreate(
        /* uxQueueLength = */ 10,
        /* uxItemSize = */ sizeof(MENU_INPUT_t));

    // If the queue failed to create, abort
    if(0 == menu_input_queue_handle)
    {
        Serial.print("Failed to create menu input queue, aborting");
        abort();
    }

    // Start task to read inputs added to queue
    // TODO: Look into static memory instead of heap memory task creation, better? Better arguments?
    //       Check return value (TaskFunction_t)
    xTaskCreate(
        // Pointer to the task entry function. Tasks must be implemented to never return (i.e. continuous loop).
        /* TaskFunction_t pxTaskCode = */ (TaskFunction_t) task_read_menu_input_queue,
        // A descriptive name for the task. This is mainly used to facilitate debugging. Max length defined by configMAX_TASK_NAME_LEN - default is 16.
        /* const char *const pcName = */ "read_menu_queue",
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

void from_isr_add_to_menu_input_queue(MENU_INPUT_t menu_input)
{
    // Write menu input GPIO pin number to queue holding all button presses
    // Remember, non-ISR queue access is not safe from within interrupts!
    xQueueSendFromISR(
        // The handle to the queue on which the item is to be posted.
        /* QueueHandle_t xQueue = */ menu_input_queue_handle,
        // A pointer to the item that is to be placed on the queue.
        // The size of the items the queue will hold was defined when the queue was created,
        // so this many bytes will be copied from pvItemToQueue into the queue storage area.
        /* const void *const pvItemToQueue = */ &menu_input,
        // xQueueGenericSendFromISR() will set *pxHigherPriorityTaskWoken to pdTRUE if sending to the queue caused a task to unblock,
        // and the unblocked task has a priority higher than the currently running task.
        // If xQueueGenericSendFromISR() sets this value to pdTRUE then a context switch should be requested before the interrupt is exited.
        /* BaseType_t *const pxHigherPriorityTaskWoken = */ NULL
    );
}

static void task_read_menu_input_queue()
{
    // Get GPIO pin number from queue holding all button presses
    MENU_INPUT_t menu_input = MENU_INPUT_NONE;

    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while (1)
    {
        // Wait for an input to enter the menu input queue
        if(xQueueReceive(
            // The handle to the queue from which the item is to be received.
            /* QueueHandle_t xQueue = */ menu_input_queue_handle,
            // Pointer to the buffer into which the received item will be copied.
            /* void *pvBuffer = */ &menu_input,
            // The maximum amount of time the task should block waiting for an item to receive should the queue be empty at the time of the call.
            // xQueueReceive() will return immediately if xTicksToWait is zero and the queue is empty.
            // The time is defined in tick periods so the constant portTICK_PERIOD_MS should be used to convert to real time if this is required.
            /* TickType_t xTicksToWait */ portMAX_DELAY))
        {
            // Depending on the button in the queue, print a different message
            menu.react_to_menu_input(/* MENU_INPUT_t menu_input = */ menu_input);
        }
    }
}

// ========================= //
// MenuLine member functions //
// ========================= //

MenuLine::MenuLine(
    char *arg_str_display,
    //void (*arg_to_str)(),
    bool (*arg_func_on_up)(),
    bool (*arg_func_on_confirm)(),
    bool (*arg_func_on_down)())
{
    str_display_has_changed = true;
    str_display = arg_str_display;
    func_on_up = arg_func_on_up;
    func_on_confirm = arg_func_on_confirm;
    func_on_down = arg_func_on_down;
}

bool MenuLine::react_to_menu_input(MENU_INPUT_t input)
{
    // Get the function to call based on the input received
    bool (*func_to_call)() = nullptr;
    switch(input)
    {
        case MENU_INPUT_UP:
            func_to_call = func_on_up;
            break;
        case MENU_INPUT_DOWN:
            func_to_call = func_on_down;
            break;
        case MENU_INPUT_CONFIRM:
            func_to_call = func_on_confirm;
            break;
        default:
            break;
    }

    // If there is not a function defined for this input, do nothing
    if (nullptr == func_to_call)
    {
        return true;
    }

    // Call the function
    return (*func_to_call)();
}

bool MenuLine::get_str(char **arg_str)
{
    *arg_str = str_display;
    bool status = str_display_has_changed;
    str_display_has_changed = false;
    return status;
}

// ===================== //
// Menu member functions //
// ===================== //

Menu::Menu(
    MenuLine *arg_menu_lines,
    size_t arg_num_menu_lines)
{
    menu_lines = arg_menu_lines;
    num_menu_lines = arg_num_menu_lines;
    index_menu_item_hover = 0;
    is_menu_item_selected = false;
}

void Menu::react_to_menu_input(MENU_INPUT_t menu_input)
{
    // If a menu item is currently selected, use its handlers for button inputs
    if(true == is_menu_item_selected)
    {
        // The menu line handler tells if after this button press, the menu should consume inputs again instead of the menu line
        is_menu_item_selected = !(menu_lines[index_menu_item_hover].react_to_menu_input(menu_input));
    }
    else
    {
        // This menu input goes to the menu, not a menu line
        switch(menu_input)
        {
            case MENU_INPUT_UP:
                Serial.println("Read menu UP input");
                index_menu_item_hover = (index_menu_item_hover == 0) ? (num_menu_lines - 1) : (index_menu_item_hover - 1);
                break;
            case MENU_INPUT_CONFIRM:
                Serial.println("Read menu CONFIRM input");
                is_menu_item_selected = true;
                break;
            case MENU_INPUT_DOWN:
                Serial.println("Read menu DOWN input");
                index_menu_item_hover = (index_menu_item_hover == (num_menu_lines - 1)) ? (0) : (index_menu_item_hover + 1);
                break;
            default:
                Serial.println("Read unrecognized menu input, will do nothing with it");
                break;
        }
    }

    // Update the display
    // TODO: Don't update per every input?
    //       If the inputs were really fast after one-another, their operations can be combined,
    //       then the display is updated again to save the number of times the function is called.
    update_display();
}

// TODO: Do it need to put a mutex lock around this? Altough seeing things glitch around would be pretty fun...
void Menu::update_display()
{
    // Here is an example of a 20x4 display.
    // In this example, X is a custom water-drip character, and # is a custom right-arrow character
    // -------------------- //
    // # Goal X: 50%        //
    //   X freq: 1000 min   //
    //   Last X: 08:00 AM   //
    //   Next X: 12:00 PM   //
    // -------------------- //

    // TODO: Is there a way to batch together display updates and send them all out at once instead?

    // Clear characters already on display
    display.clear();

    // Display cursor
    display.setCursor(
        /* uint8_t col = */ 0,
        /* uint8_t row = */ 0
    );
    display.print(/* const char *c = */ is_menu_item_selected ? "#" : ">");

    // Display menu lines
    // TODO: use #define or varaible to hold number of lines in the display
    if (0 == num_menu_lines)
    {
        return;
    }
    char *menu_line_str = nullptr;
    for(size_t line_num = 0; line_num < 4; ++line_num)
    {
        // Set the cursor
        display.setCursor(
            /* uint8_t col = */ 2,
            /* uint8_t row = */ line_num
        );

        // Get the menu line as a string, put it onto the screen
        (void) menu_lines[(index_menu_item_hover + line_num) % num_menu_lines].get_str(/* char **arg_str = */ &menu_line_str);
        if(nullptr != menu_line_str)
        {
            display.print(/* const char *c = */ menu_line_str);
        }
    }
}