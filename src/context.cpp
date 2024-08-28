// TODO: comment
#include "context.h"
// TODO: comment
#include "menu.h"

// ======================================= //
// Define reusable tasks, interrupts, etc. //
// ======================================= //

void task_rotate_servo(Servo *servo)
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

        // Turn the servo to neutral position, wait a second, if it is not already in it
        if (0 != servo->read())
        {
            servo->write(/* int value = */ 0);
            vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));
        }

        // Turn the servo to opposite of neutral position, wait a second
        // TODO: This seems to turn a but more than 180 degrees, calibrate it using the library?
        servo->write(/* int value = */ 90);
        vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));

        // Turn the servo to neutral position, wait a second
        servo->write(/* int value = */ 0);
        vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));

        // Turn back on screen and other peripherals
        // NOTE: lol this destroys the display when I don't have an external power supply nevermind
        //vTaskResume(/*TaskHandle_t xTaskToResume = */ get_toggle_sleep_mode_task_handle());
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

    // Return control to the menu
    return true;
}

bool Context::spray()
{
    vTaskResume(/* TaskHandle_t xTaskToResume = */ rotate_servo_task_handle);

    // Return control to the menu
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

    // Alter the desired humidty and account for overflow
    percent_desired_humidity = (percent_desired_humidity >= 100) ? 0 : (percent_desired_humidity + 1);

    // Free the mutex
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    // Do not return control to the menu
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

    // Alter the humidity check frequenecy and next humidity check time
    ++minute_humidity_check_freq;
    time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

    // Free the mutex
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    // Do not return control to the menu
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

    // Alter the desired humidty and account for underflow
    percent_desired_humidity = (percent_desired_humidity <= 0) ? 100 : (percent_desired_humidity - 1);

    // Free the mutex
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    // Do not return control to the menu
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

    // Alter the humidity check frequenecy and next humidity check time
    --minute_humidity_check_freq;
    time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

    // Free the mutex
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    // Do not return control to the menu
    return false;
}

String Context::str_percent_desired_humidity()
{
    // -------------------- //
    //   Goal X: 100%       //
    // -------------------- //
    return String("Goal X: " + String(percent_desired_humidity, DEC) + "%");
}

String Context::str_minute_humidity_check_freq()
{
    // -------------------- //
    //   X freq: 100%       //
    // -------------------- //
    return String("X freq: " + String(minute_humidity_check_freq, DEC) + " min");
}

String Context::str_time_last_humidity_check()
{
    // -------------------- //
    //   Last X: Fri 13:00  //
    // -------------------- //
    char line[NUM_DISPLAY_COLUMNS - sizeof('>') - sizeof(' ') + sizeof('\0')] = { 0 };
    // https://en.cppreference.com/w/cpp/chrono/c/strftime
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
    //   Next X: Fri 13:00  //
    // -------------------- //
    char line[NUM_DISPLAY_COLUMNS - sizeof('>') - sizeof(' ') + sizeof('\0')] = { 0 };
    // https://en.cppreference.com/w/cpp/chrono/c/strftime
    strftime(
        /* char* str = */ line,
        /* std::size_t count = */ sizeof(line),
        /* const char* format = */ "%a %H:%M",
        // TODO: Comment args
        /* const std::tm* tp = */ localtime(&time_next_humidity_check));
    return String("Next X: " + String(line));
}