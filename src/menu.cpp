// Include custom menu class implementation
#include "menu.h"
// Include FreeRTOS queue API
#include "freertos/queue.h"

// Define the number of lines in menu_lines
#define NUM_MENU_LINES 6

// Define I2C address for display, see your manufacturer notes to figure out yours
#define DISPLAY_I2C_ADDR 0x27
// Define the number of rows in the LCD display (the number of lines of characters)
#define NUM_DISPLAY_ROWS 4
// Define the number of columns in the LCD display (the number of characters in each line)
#define NUM_DISPLAY_COLUMNS 20

// ======================= //
// Instantiate useful data //
// ======================= //

// TODO: Make these members of Menu, so you can have multiple menus?
QueueHandle_t menu_input_queue_handle;

// Store the handle of the task reading the menu input queue
TaskHandle_t read_menu_input_queue_task_handle;

// Define statically allocated buffer for context mutex
StaticSemaphore_t context_mutex_buffer;

// Initialize display peripheral
LiquidCrystal_I2C display(
    /* uint8_t lcd_Addr = */ DISPLAY_I2C_ADDR,
    /* uint8_t lcd_cols = */ NUM_DISPLAY_COLUMNS,
    /* uint8_t lcd_rows = */ NUM_DISPLAY_ROWS
);

// Create an instance of a context
static Context context = { 
    /* StaticSemaphore_t *mutex_buffer = */ &context_mutex_buffer,
    /* int pin_servo_out = */ PIN_SERVO_OUT
};

// Define the lines within the menu
// Using C++ lambdas: https://en.cppreference.com/w/cpp/language/lambda
// TODO: make context an argument so multiple contexts can be controled by this menu
static MenuLine menu_lines[NUM_MENU_LINES] = 
{
    {
        /* String str_display = */ String("Goal X: 50%"),
        /* String (*arg_func_to_str)() = */ []() { return context.str_percent_desired_humidity(); },
        /* bool (*arg_func_on_up)() = */ []() { return context.add_percent_desired_humidity(); },
        /* bool (*arg_func_on_confirm)() = */ nullptr,
        /* bool (*arg_func_on_down)() = */ []() { return context.subtract_percent_desired_humidity(); }
    },
    {
        /* String str_display = */ String("X freq: 1000 min"),
        /* String (*arg_func_to_str)() = */ []() { return context.str_minute_humidity_check_freq(); },
        /* bool (*arg_func_on_up)() = */ []() { return context.add_minute_humidity_check_freq(); },
        /* bool (*arg_func_on_confirm)() = */ nullptr,
        /* bool (*arg_func_on_down)() = */ []() { return context.subtract_minute_humidity_check_freq(); }
    },
    {
        /* String str_display = */ String("Last X: 08:00 AM"),
        /* String (*arg_func_to_str)() = */ []() { return context.str_time_last_humidity_check(); },
        /* bool (*arg_func_on_up)() = */ nullptr,
        /* bool (*arg_func_on_confirm)() = */ nullptr,
        /* bool (*arg_func_on_down)() = */ nullptr
    },
    {
        /* String str_display = */ String("Next X: 12:00 PM"),
        /* String (*arg_func_to_str)() = */ []() { return context.str_time_next_humidity_check(); },
        /* bool (*arg_func_on_up)() = */ nullptr,
        /* bool (*arg_func_on_confirm)() = */ nullptr,
        /* bool (*arg_func_on_down)() = */ nullptr
    },
    {
        /* String str_display = */ String("Check X"),
        /* String (*arg_func_to_str)() = */ nullptr,
        /* bool (*arg_func_on_up)() = */ nullptr,
        /* bool (*arg_func_on_confirm)() = */ []() { return context.check_humidity(); },
        /* bool (*arg_func_on_down)() = */ nullptr
    },
    {
        /* String str_display = */ String("Trigger spray"),
        /* String (*arg_func_to_str)() = */ nullptr,
        /* bool (*arg_func_on_up)() = */ nullptr,
        /* bool (*arg_func_on_confirm)() = */ []() { return context.spray(); },
        /* bool (*arg_func_on_down)() = */ nullptr
    }
};

// Create an instance of a menu
static Menu menu = {
    /* MenuLine *arg_menu_lines = */ menu_lines,
    /* size_t arg_num_menu_lines = */ NUM_MENU_LINES
};

// ======================== //
// Define custom characters //
// ======================== //

// Define an enum to keep track of custom character mapping
// See here for defining custom characters:
// https://lastminuteengineers.com/esp32-i2c-lcd-tutorial/
enum CUSTOM_CHAR_t : uint8_t
{
    CUSTOM_CHAR_NONE = 0,
    CUSTOM_CHAR_WATER_DROP,
    CUSTOM_CHAR_FILLED_RIGHT_ARROW,
    CUSTOM_CHAR_MAX
};

// Define custom character for water emoji, X in the display examples
byte custom_char_water_drop[] = {
    0b00100,
    0b01110,
    0b01110,
    0b11111,
    0b11111,
    0b11111,
    0b01110,
    0b00000,
};

// Define custom character for filled right-arrow emoji, # in display examples
byte custom_char_filled_right_arrow[] = {
    0b01000,
    0b01100,
    0b01110,
    0b01111,
    0b01110,
    0b01100,
    0b01000,
    0b00000,
};

// =================================================== //
// Functions for interacting with the menu input queue //
// =================================================== //

LiquidCrystal_I2C *get_display()
{
    return &display;
}

TaskHandle_t get_read_menu_input_queue_task_handle()
{
    return read_menu_input_queue_task_handle;
}

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

    // Register custom characters
    // TODO: Use water character. By having a menu line not hold a string, but write to the screen itself? Or can they be passed in print?
    display.createChar(CUSTOM_CHAR_WATER_DROP, custom_char_water_drop);
    display.createChar(CUSTOM_CHAR_FILLED_RIGHT_ARROW, custom_char_filled_right_arrow);

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
        /* TaskHandle_t *const pxCreatedTask = */ &read_menu_input_queue_task_handle);
    configASSERT(read_menu_input_queue_task_handle);
}

// Use non-member function, so many sources can write to the same input queue
// Bound functions, such as member functions, can only be called, not used as pointers
void from_isr_add_to_menu_input_queue(MENU_INPUT_t menu_input)
{
    // Write menu input GPIO pin number to queue holding all button presses
    // Remember, non-ISR queue access is not safe from within interrupts!
    // Don't care about the return value, I am ok with losing some button inputs
    (void) xQueueSendFromISR(
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

// Use non-member function, so multiple menus can read from the same input queue
static void task_read_menu_input_queue()
{
    // Get GPIO pin number from queue holding all button presses
    MENU_INPUT_t menu_input = MENU_INPUT_NONE;

    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while(1)
    {
        // Wait for an input to enter the menu input queue
        if(pdTRUE == xQueueReceive(
            // The handle to the queue from which the item is to be received.
            /* QueueHandle_t xQueue = */ menu_input_queue_handle,
            // Pointer to the buffer into which the received item will be copied.
            /* void *pvBuffer = */ &menu_input,
            // The maximum amount of time the task should block waiting for an item to receive should the queue be empty at the time of the call.
            // xQueueReceive() will return immediately if xTicksToWait is zero and the queue is empty.
            // The time is defined in tick periods so the constant portTICK_PERIOD_MS should be used to convert to real time if this is required.
            /* TickType_t xTicksToWait */ portMAX_DELAY))
        {
            // Use the menu input to manipulated the menu
            menu.react_to_menu_input(/* MENU_INPUT_t menu_input = */ menu_input);
        }
    }
}

static void task_rotate_servo(Servo *servo)
{
    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while(1)
    {
        // Suspend this task until it is needed
        vTaskSuspend(/* TaskHandle_t xTaskToSuspend = */ NULL);

        // Turn off screen an other peripherals to save power
        // NOTE: lol this destroys the display when I don't have an external power supply nevermind
        //vTaskResume(/*TaskHandle_t xTaskToResume = */ get_toggle_sleep_mode_task_handle());

        // TODO: put inside of task that can yield, add mutex for servo?
        // Turn the servo to neutral position, wait a second, if it is not already in it
        if (0 != servo->read())
        {
            servo->write(/* int value = */ 0);
            vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));
        }

        // Turn the servo to opposite of neutral position, wait a second
        // TODO: This seems to turn a but more than 180 degrees, calibrate it using the library?
        servo->write(/* int value = */ 180);
        vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));

        // Turn the servo to neutral position, wait a second
        servo->write(/* int value = */ 0);
        vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));

        // Turn back on screen and other peripherals
        // NOTE: lol this destroys the display when I don't have an external power supply nevermind
        //vTaskResume(/*TaskHandle_t xTaskToResume = */ get_toggle_sleep_mode_task_handle());
    }
}

// ========================= //
// MenuLine member functions //
// ========================= //

MenuLine::MenuLine(
    String arg_str_display,
    String (*arg_func_to_str)(),
    bool (*arg_func_on_up)(),
    bool (*arg_func_on_confirm)(),
    bool (*arg_func_on_down)())
{
    str_display = arg_str_display;
    func_to_str = arg_func_to_str;
    is_str_and_data_desynced = (arg_func_to_str != nullptr);
    func_on_up = arg_func_on_up;
    func_on_confirm = arg_func_on_confirm;
    func_on_down = arg_func_on_down;
}

bool MenuLine::react_to_menu_input(MENU_INPUT_t input)
{
    // Get the function to call based on the input received
    // NOTE: can use hashmap instead of switch statement, ex. func_to_call = funcs[MENU_INPUT_UP]
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
    bool status = (*func_to_call)();

    // Record whether the display is not showing the current updates
    // TODO: Pass is_str_and_data_desynced as argument instead so functions can set this?
    if ((MENU_INPUT_UP == input) ||
        (MENU_INPUT_DOWN == input))
    {
        is_str_and_data_desynced = true;
    }

    return status;
}

bool MenuLine::get_str(String **arg_str)
{
    // Only generate a new string if there's a function to do it and a change has happened to the underlying data
    if ((nullptr != func_to_str) &&
        (true == is_str_and_data_desynced))
    {
        str_display = (*func_to_str)();
    }
    *arg_str = &str_display;
    bool status = is_str_and_data_desynced;
    is_str_and_data_desynced = false;
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
                index_menu_item_hover = (index_menu_item_hover == 0) ? (num_menu_lines - 1) : (index_menu_item_hover - 1);
                break;
            case MENU_INPUT_CONFIRM:
                is_menu_item_selected = true;
                break;
            case MENU_INPUT_DOWN:
                index_menu_item_hover = (index_menu_item_hover == (num_menu_lines - 1)) ? (0) : (index_menu_item_hover + 1);
                break;
            default:
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
    if(true == is_menu_item_selected)
    {
        display.write(/* uint8_t = */ CUSTOM_CHAR_FILLED_RIGHT_ARROW);
    }
    else
    {
        display.print(/* const char *c = */ ">");
    }

    // Display menu lines
    if (0 == num_menu_lines)
    {
        return;
    }
    String *menu_line_str = nullptr;
    for(size_t line_num = 0; line_num < NUM_DISPLAY_ROWS; ++line_num)
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
            display.print(/* const char *c = */ menu_line_str->c_str());
        }
    }
}

// ======================== //
// Context member functions //
// ======================== //

Context::Context(
    StaticSemaphore_t *mutex_buffer,
    int pin_servo_out)
{
    // Create mutex, open it for grabbing
    mutex_handle = xSemaphoreCreateMutexStatic(/* pxMutexBuffer = */ mutex_buffer);
    assert(nullptr != mutex_handle);
    xSemaphoreGive(/* xSempahore = */ mutex_handle);

    // Attach servo
    servo.attach(/* int pin = */ pin_servo_out);

    // Create task to rotate servo motor an interupt can 'call'
    // TODO: Look into static memory instead of heap memory task creation, better? Better arguments?
    xTaskCreate(
        // Pointer to the task entry function. Tasks must be implemented to never return (i.e. continuous loop).
        /* TaskFunction_t pxTaskCode = */ (TaskFunction_t) task_rotate_servo,
        // A descriptive name for the task. This is mainly used to facilitate debugging. Max length defined by configMAX_TASK_NAME_LEN - default is 16.
        /* const char *const pcName = */ "rotate_servo",
        // The size of the task stack specified as the NUMBER OF BYTES. Note that this differs from vanilla FreeRTOS.
        /* const configSTACK_DEPT_TYPE usStackDepth = */ 2048,
        // Pointer that will be used as the parameter for the task being created.
        /* void *const pvParameters = */ &servo,
        // The priority at which the task should run.
        // Systems that include MPU support can optionally create tasks in a privileged (system) mode by setting bit portPRIVILEGE_BIT of the priority parameter.
        // For example, to create a privileged task at priority 2 the uxPriority parameter should be set to ( 2 | portPRIVILEGE_BIT ).
        /* UBaseType_t uxPriority = */ 10,
        // Used to pass back a handle by which the created task can be referenced.
        /* TaskHandle_t *const pxCreatedTask = */ &rotate_servo_task_handle);
    configASSERT(rotate_servo_task_handle);

    // TODO: read setting from previous run
    percent_desired_humidity = 25;
    // TODO: read setting from previous run
    minute_humidity_check_freq = 100;

    check_humidity();
}

bool Context::check_humidity()
{
    // Wait until the mutex is free, or 1000 milliseconds
    // If the mutex could not be taken within 1000 milliseconds, don't do anything
    if(pdFALSE == xSemaphoreTake(
        /* xSemaphore = */ mutex_handle,
        /* xBlockTime = */ pdMS_TO_TICKS(1000)))
    {
        return true;
    }

    // TODO: read the actual humidity
    percent_current_humidity = 25;
    time_last_humidity_check = time(/* time_t *arg = */ nullptr);
    time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return true;
}

bool Context::spray()
{
    vTaskResume(/* TaskHandle_t xTaskToResume = */ rotate_servo_task_handle);
    return true;
}

bool Context::add_percent_desired_humidity()
{
    // Wait until the mutex is free, or 1000 milliseconds
    // If the mutex could not be taken within 1000 milliseconds, don't do anything
    if(pdFALSE == xSemaphoreTake(
        /* xSemaphore = */ mutex_handle,
        /* xBlockTime = */ pdMS_TO_TICKS(1000)))
    {
        return false;
    }

    percent_desired_humidity = (percent_desired_humidity >= 100) ? 0 : (percent_desired_humidity + 1);

    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return false;
}

bool Context::add_minute_humidity_check_freq()
{
    // Wait until the mutex is free, or 1000 milliseconds
    // If the mutex could not be taken within 1000 milliseconds, don't do anything
    if(pdFALSE == xSemaphoreTake(
        /* xSemaphore = */ mutex_handle,
        /* xBlockTime = */ pdMS_TO_TICKS(1000)))
    {
        return false;
    }

    ++minute_humidity_check_freq;
    time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return false;
}

bool Context::subtract_percent_desired_humidity()
{
    // Wait until the mutex is free, or 1000 milliseconds
    // If the mutex could not be taken within 1000 milliseconds, don't do anything
    if(pdFALSE == xSemaphoreTake(
        /* xSemaphore = */ mutex_handle,
        /* xBlockTime = */ pdMS_TO_TICKS(1000)))
    {
        return false;
    }

    percent_desired_humidity = (percent_desired_humidity <= 0) ? 100 : (percent_desired_humidity - 1);

    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return false;
}

bool Context::subtract_minute_humidity_check_freq()
{
    // Wait until the mutex is free, or 1000 milliseconds
    // If the mutex could not be taken within 1000 milliseconds, don't do anything
    if(pdFALSE == xSemaphoreTake(
        /* xSemaphore = */ mutex_handle,
        /* xBlockTime = */ pdMS_TO_TICKS(1000)))
    {
        return false;
    }

    --minute_humidity_check_freq;
    time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return false;
}

String Context::str_percent_desired_humidity()
{
    return String("Goal X: " + String(percent_desired_humidity, DEC) + "%");
}

String Context::str_minute_humidity_check_freq()
{
    return String("X freq: " + String(minute_humidity_check_freq, DEC) + " min");
}

String Context::str_time_last_humidity_check()
{
    // -------------------- //
    //   Last X: Fri 08:00  //
    // -------------------- //
    // https://en.cppreference.com/w/cpp/chrono/c/strftime
    char line[NUM_DISPLAY_COLUMNS - sizeof('>') - sizeof(' ') + sizeof('\0')] = { 0 };
    strftime(
        /* char* str = */ line,
        /* std::size_t count = */ sizeof(line),
        /* const char* format = */ "%a %H:%M",
        // TODO: Comment args
        /* const std::tm* tp = */ localtime(&time_last_humidity_check));
    return String("Last X: " + String(line));
}

String Context::str_time_next_humidity_check()
{
    // -------------------- //
    //   Next X: Fri 08:00  //
    // -------------------- //
    // https://en.cppreference.com/w/cpp/chrono/c/strftime
    char line[NUM_DISPLAY_COLUMNS - sizeof('>') - sizeof(' ') + sizeof('\0')] = { 0 };
    strftime(
        /* char* str = */ line,
        /* std::size_t count = */ sizeof(line),
        /* const char* format = */ "%a %H:%M",
        // TODO: Comment args
        /* const std::tm* tp = */ localtime(&time_next_humidity_check));
    return String("Next X: " + String(line));
}