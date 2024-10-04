// Include custom Context class implementation
#include "context.h"
// Include custom Menu class implementation
#include "menu.h"
// Include custom debug macros and compile flags
#include "flags.h"

// ======================================= //
// Define reusable tasks, interrupts, etc. //
// ======================================= //

// TODO: Is this task really necessary? Can it be absorbed into task_water?
void task_rotate_servo(Servo *servo)
{
    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while(1)
    {
        // Suspend this task until it is needed
        vTaskSuspend(/* TaskHandle_t xTaskToSuspend = */ NULL);

        // Turn the servo to neutral position, wait a second, if it is not already in it
        if (0 != servo->read())
        {
            servo->write(/* int value = */ 0);
            vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));
        }

        // Turn the servo to opposite of neutral position, wait a second
        // TODO: This seems to turn a bit more than 90 degrees, calibrate it using the library?
        servo->write(/* int value = */ 90);
        vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));

        // Turn the servo to neutral position, wait a second
        servo->write(/* int value = */ 0);
        vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(2000));

        // 19SEP2024: usStackDepth = 1024, uxTaskGetHighWaterMark = 352
        PRINT_STACK_USAGE();
    }
}

// Read the soil humidity sensor, while the humidity is below our desired threshold, add more water
void task_water(Context *context)
{
    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while(1)
    {
        // Wait until it is the time for the next humidity check
        // TODO: Is there a more efficient way to do this?
        //       Maybe have a timer task that simply resumes or notifies this task?
        while(false == context->is_humidity_check_overdue())
        {
            // Wait 1 minute
            vTaskDelay(/* TickType_t xTicksToDelay = */ 60 * 1000);
        }

        // Update context's current humidity, time last checked, and time of next check
        // if we fail, oh well, not really worth waiting until it works
        (void) context->check_humidity();

        // While we are not at our desired humidity, add more water
        // TODO: Optimize the number of sensor polls needed.
        //       1. Do, say, 10 squirts. See how much it increases the soil humidity, find the average from that sample size.
        //       2. Poll soil humidity
        //       3. Use the earlier average to calculate how many squirts are needed. Do that many squirts.
        //       4. Repeat from 2. This reduces the number of times the sensor needs to be polled, reducing corrosion.
        while(true == context->is_current_humidity_below_desired())
        {
            // Trigger servo motor to squirt, wait until it finishes
            context->spray(
                /* bool in_isr = */ false,
                /* bool is_blocking = */ true);

            // Wait 5 seconds for water from squirt to soak into soil
            vTaskDelay(/* TickType_t xTicksToDelay = */ pdMS_TO_TICKS(5000));

            // Update reading
            // if we fail, oh well, not really worth waiting until it works
            (void) context->check_humidity();
        }

        // 24AUG2024: usStackDepth = 2048, uxTaskGetHighWaterMark = ???
        PRINT_STACK_USAGE();
    }
}

// ======================== //
// Context member functions //
// ======================== //

Context::Context(
    StaticSemaphore_t *arg_mutex_buffer,
    int arg_pin_servo_out,
    gpio_num_t arg_pin_soil_moisture_sensor_in)
{
    // Create mutex, open it for grabbing
    mutex_handle = xSemaphoreCreateMutexStatic(/* pxMutexBuffer = */ arg_mutex_buffer);
    assert(nullptr != mutex_handle);
    xSemaphoreGive(/* xSempahore = */ mutex_handle);

    // Attach servo
    servo.attach(/* int pin = */ arg_pin_servo_out);

    // Remember the pin of the soil moisture sensor
    pin_soil_moisture_sensor_in = arg_pin_soil_moisture_sensor_in;

    // Create task to rotate servo motor an interrupt can 'call'
    // TODO: Look into static memory allocation instead?
    xTaskCreate(
        // Pointer to the task entry function. Tasks must be implemented to never return (i.e. continuous loop).
        /* TaskFunction_t pxTaskCode = */ (TaskFunction_t) task_rotate_servo,
        // A descriptive name for the task. This is mainly used to facilitate debugging. Max length defined by configMAX_TASK_NAME_LEN - default is 16.
        /* const char *const pcName = */ "rotate_servo",
        // The size of the task stack specified as the NUMBER OF BYTES. Note that this differs from vanilla FreeRTOS.
        /* const configSTACK_DEPT_TYPE usStackDepth = */ 1024,
        // Pointer that will be used as the parameter for the task being created.
        /* void *const pvParameters = */ &servo,
        // The priority at which the task should run.
        // Systems that include MPU support can optionally create tasks in a privileged (system) mode by setting bit portPRIVILEGE_BIT of the priority parameter.
        // For example, to create a privileged task at priority 2 the uxPriority parameter should be set to ( 2 | portPRIVILEGE_BIT ).
        /* UBaseType_t uxPriority = */ 10,
        // Used to pass back a handle by which the created task can be referenced.
        /* TaskHandle_t *const pxCreatedTask = */ &rotate_servo_task_handle);
    configASSERT(rotate_servo_task_handle);

    // Create task to schedule sprays
    // TODO: Look into static memory allocation instead?
    xTaskCreate(
        // Pointer to the task entry function. Tasks must be implemented to never return (i.e. continuous loop).
        /* TaskFunction_t pxTaskCode = */ (TaskFunction_t) task_water,
        // A descriptive name for the task. This is mainly used to facilitate debugging. Max length defined by configMAX_TASK_NAME_LEN - default is 16.
        /* const char *const pcName = */ "water",
        // The size of the task stack specified as the NUMBER OF BYTES. Note that this differs from vanilla FreeRTOS.
        /* const configSTACK_DEPT_TYPE usStackDepth = */ 2048,
        // Pointer that will be used as the parameter for the task being created.
        /* void *const pvParameters = */ this,
        // The priority at which the task should run.
        // Systems that include MPU support can optionally create tasks in a privileged (system) mode by setting bit portPRIVILEGE_BIT of the priority parameter.
        // For example, to create a privileged task at priority 2 the uxPriority parameter should be set to ( 2 | portPRIVILEGE_BIT ).
        /* UBaseType_t uxPriority = */ 10,
        // Used to pass back a handle by which the created task can be referenced.
        /* TaskHandle_t *const pxCreatedTask = */ &water_task_handle);
    configASSERT(water_task_handle);

    // TODO: read setting from previous run
    desired_humidity = 25;
    // TODO: read setting from previous run
    minute_humidity_check_freq = 100;

    check_humidity();
}

bool Context::is_humidity_check_overdue()
{
    // Take the Context sempahore to prevent miscellaneous reads and writes from other threads
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 1000))
    {
        return false;
    }

    // If the time of the next check is after the current time, we're overdue
    bool is_overdue = time(/* time_t *arg = */ nullptr) >= time_next_humidity_check;

    // Return mutex so other threads can read and write to context members again
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return is_overdue;
}

bool Context::is_current_humidity_below_desired()
{
    // Take the Context sempahore to prevent miscellaneous reads and writes from other threads
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 1000))
    {
        return false;
    }

    bool is_current_below_desired = current_humidity < desired_humidity;

    // Return mutex so other threads can read and write to context members again
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return is_current_below_desired;
}

bool Context::check_humidity()
{
    // Take the Context sempahore to prevent miscellaneous reads and writes from other threads
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 1000))
    {
        return false;
    }

    // Update context's current humidity, time last checked, and time of next check
    current_humidity = analogRead(pin_soil_moisture_sensor_in);
    time_last_humidity_check = time(/* time_t *arg = */ nullptr);
    time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

    // Return mutex so other threads can read and write to context members again
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);
    return true;
}

bool Context::spray(
    bool in_isr,
    bool is_blocking)
{
    // Run task_rotate_servo
    if(true == in_isr)
    {
        (void) xTaskResumeFromISR(/* TaskHandle_t xTaskToResume = */ rotate_servo_task_handle);
    }
    else
    {
        vTaskResume(/* TaskHandle_t xTaskToResume = */ rotate_servo_task_handle);
    }

    // If we wanted to block (yield) until the task finishes, do so
    if(true == is_blocking)
    {
        while(eSuspended != eTaskGetState(rotate_servo_task_handle))
        {
            // Wait half a second to check if task is suspended again
            vTaskDelay(/* TickType_t xTicksToDelay = */ pdMS_TO_TICKS(500));
        }
    }

    // Return control to the menu
    return true;
}

bool Context::get(
    CONTEXT_MEMBER_t member,
    void **data,
    size_t *num_bytes_data)
{
    switch(member)
    {
        case CONTEXT_MEMBER_MUTEX_HANDLE:
            *data = &mutex_handle;
            *num_bytes_data = sizeof(mutex_handle);
            break;
        case CONTEXT_MEMBER_SERVO:
            *data = &servo;
            *num_bytes_data = sizeof(servo);
            break;
        case CONTEXT_MEMBER_ROTATE_SERVO_TASK_HANDLE:
            *data = &rotate_servo_task_handle;
            *num_bytes_data = sizeof(rotate_servo_task_handle);
            break;
        case CONTEXT_MEMBER_PIN_SOIL_MOISTURE_SENSOR_IN:
            *data = &pin_soil_moisture_sensor_in;
            *num_bytes_data = sizeof(pin_soil_moisture_sensor_in);
            break;
        case CONTEXT_MEMBER_WATER_TASK_HANDLE:
            *data = &water_task_handle;
            *num_bytes_data = sizeof(water_task_handle);
            break;
        case CONTEXT_MEMBER_CURRENT_HUMIDITY:
            *data = &current_humidity;
            *num_bytes_data = sizeof(current_humidity);
            break;
        case CONTEXT_MEMBER_DESIRED_HUMIDITY:
            *data = &desired_humidity;
            *num_bytes_data = sizeof(desired_humidity);
            break;
        case CONTEXT_MEMBER_MINUTE_HUMIDITY_CHECK_FREQ:
            *data = &minute_humidity_check_freq;
            *num_bytes_data = sizeof(minute_humidity_check_freq);
            break;
        case CONTEXT_MEMBER_TIME_LAST_HUMIDITY_CHECK:
            *data = &time_last_humidity_check;
            *num_bytes_data = sizeof(time_last_humidity_check);
            break;
        case CONTEXT_MEMBER_TIME_NEXT_HUMIDITY_CHECK:
            *data = &time_next_humidity_check;
            *num_bytes_data = sizeof(time_next_humidity_check);
            break;
        default:
            return false;
    }
    return true;
}

bool Context::copy(
    CONTEXT_MEMBER_t member,
    void *dst,
    size_t num_bytes_dst)
{
    // Initialize some local variables
    // The source is the Context member, and the destination is the function arguments
    void *src = nullptr;
    size_t num_bytes_src = 0;
    bool status = false;

    // Take the Context sempahore to assure the member being gotten isn't being modified while it is being copied
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 1000))
    {
        return false;
    }

    // Get the member's address and length, if it exists and can fit in the destination buffer, copy it in
    if((true == get(
        /* CONTEXT_MEMBER_t member = */ member,
        /* void **data = */ &src,
        /* size_t *num_bytes_data = */ &num_bytes_src)) &&
        (num_bytes_src <= num_bytes_dst))
    {
        status = true;
        (void) memcpy(
            /* void* dest = */ dst,
            /* const void* src = */ src,
            /* std::size_t count = */ num_bytes_src);
    }

    // Return mutex so other threads can read and write to context members again
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    return status;
}

bool Context::set(
    CONTEXT_MEMBER_t member,
    void *src,
    size_t num_bytes_src)
{
    // Initialize some local variables
    void *dst = nullptr;
    size_t num_bytes_dst = 0;
    bool status = false;

    // Take the Context sempahore to assure the member being gotten isn't being modified while it is being copied
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 1000))
    {
        return false;
    }

    // Get the member's address and length, if it exists and can fit the source buffer, set it to that value plus 0-padding
    if((true == get(
        /* CONTEXT_MEMBER_t member = */ member,
        /* void **data = */ &dst,
        /* size_t *num_bytes_data = */ &num_bytes_dst)) &&
        (num_bytes_src <= num_bytes_dst))
    {
        status = true;
        (void) memcpy(
            /* void* dest = */ dst,
            /* const void* src = */ src,
            /* std::size_t count = */ num_bytes_src);
        (void) memset(
            /* void* dest = */ ((uint8_t *) dst) + num_bytes_src,
            /* int ch = */ 0,
            /* std::size_t count = */ num_bytes_dst - num_bytes_src);
    }

    // Return mutex so other threads can read and write to context members again
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    return status;
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
    char line[NUM_DISPLAY_CHARS_PER_LINE - sizeof('>') - sizeof(' ') + sizeof('\0')] = { 0 };
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
    char line[NUM_DISPLAY_CHARS_PER_LINE - sizeof('>') - sizeof(' ') + sizeof('\0')] = { 0 };
    // https://en.cppreference.com/w/cpp/chrono/c/strftime
    strftime(
        /* char* str = */ line,
        /* std::size_t count = */ sizeof(line),
        /* const char* format = */ "%a %H:%M",
        // TODO: Comment args
        /* const std::tm* tp = */ localtime(&time_next_humidity_check));
    return String("Next X: " + String(line));
}