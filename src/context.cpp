// Include custom Context class implementation
#include "context.h"
// Include custom Menu class implementation
#include "menu.h"
// Include custom debug macros and compile flags
#include "flags.h"

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

        // Turn off screen and other peripherals to save power
        // NOTE: lol this destroys the display when I don't have an external power supply nevermind
        //vTaskResume(/*TaskHandle_t xTaskToResume = */ get_toggle_sleep_mode_task_handle());

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

        // Turn back on screen and other peripherals
        // NOTE: lol this destroys the display when I don't have an external power supply nevermind
        //vTaskResume(/*TaskHandle_t xTaskToResume = */ get_toggle_sleep_mode_task_handle());

        // 19SEP2024: usStackDepth = 1024, uxTaskGetHighWaterMark = 352
        PRINT_STACK_USAGE();
    }
}

// ... Oh, how I wish member functions could be used as tasks
// TODO: I don't have a sensor to run this code because of supply chain issues.
//       So, this function isn't currently enabled in any way, it's just created and suspends without anyone to resume it.
//       Test it by making what would be a real reading replaced by a random number that increments?
void task_water(Context *context)
{
    // NOTE: Only percent_desired_moisture can change after initialization in the
    // current Context implementation, please update this code if that changes
    gpio_num_t pin = GPIO_NUM_0;
    TaskHandle_t rotate_servo_task_handle;
    int percent_desired_humidity = 0;
    configASSERT(context->get(
        /* enum CONTEXT_MEMBER_t = */ CONTEXT_MEMBER_PIN_SOIL_MOISTURE_SENSOR_IN,
        /* void *dst = */ (void *) &pin,
        /* size_t num_bytes_dst = */ sizeof(pin)));
    configASSERT(context->get(
            /* enum CONTEXT_MEMBER_t = */ CONTEXT_MEMBER_ROTATE_SERVO_TASK_HANDLE,
            /* void *dst = */ (void *) &rotate_servo_task_handle,
            /* size_t num_bytes_dst = */ sizeof(rotate_servo_task_handle)));

    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    while(1)
    {
        // Suspend this task until it is needed
        vTaskSuspend(/* TaskHandle_t xTaskToSuspend = */ NULL);

        (void) context->get(
            /* enum CONTEXT_MEMBER_t = */ CONTEXT_MEMBER_PERCENT_DESIRED_HUMIDITY,
            /* void *dst = */ (void *) &percent_desired_humidity,
            /* size_t num_bytes_dst = */ sizeof(percent_desired_humidity));

        // While the moisture is below desired_mositure, squirt then wait one second
        // TODO: Optimize the number of sensor polls needed.
        //       1. Do, say, 10 squirts. See how much it increases the soil humidity, find the average from that sample size.
        //       2. Poll soil humidity
        //       3. Use the earlier average to calculate how many squirts are needed. Do that many squirts.
        //       4. Repeat from 2. This reduces the number of times the sensor needs to be polled, reducing corrosion.
        while(analogRead(pin) < percent_desired_humidity)
        {
            // Trigger servo motor to squirt
            vTaskResume(/* TaskHandle_t xTaskToResume = */ rotate_servo_task_handle);

            // Wait until it finishes
            // TODO: is there a better way to do this, such as Notify?
            while(eSuspended != eTaskGetState(rotate_servo_task_handle))
            {
                // Wait one second to check id task is suspended again
                vTaskDelay(pdMS_TO_TICKS(1000));
            }

            // Wait 5 seconds for water from squirt to soak into soil
            vTaskDelay(pdMS_TO_TICKS(5000));

            // TODO: update context->percent_current_humidity = percent_current_humidity
            //       update context->time_last_humidity_check = time(/* time_t *arg = */ nullptr);
            //       update context->time_next_humidity_check = time_last_humidity_check + (minute_humidity_check_freq * 60);

            // Another thread may have updated percent_desired_humidity, get it, if possible
            (void) context->get(
                /* enum CONTEXT_MEMBER_t = */ CONTEXT_MEMBER_PERCENT_DESIRED_HUMIDITY,
                /* void *dst = */ (void *) &percent_desired_humidity,
                /* size_t num_bytes_dst = */ sizeof(percent_desired_humidity));
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

    // Create task to rotate servo motor an interupt can 'call'
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

    // Create task to get to the desired humidity an interupt can 'call'
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
    percent_desired_humidity = 25;
    // TODO: read setting from previous run
    minute_humidity_check_freq = 100;

    check_humidity(/* bool in_isr = */ false);
}

bool Context::check_humidity(bool in_isr)
{
    // Run task_water
    // TODO: properly schedule it at time_next_humidity_check
    if(true == in_isr)
    {
        (void) xTaskResumeFromISR(/* TaskHandle_t xTaskToResume = */ water_task_handle);
    }
    else
    {
        vTaskResume(/* TaskHandle_t xTaskToResume = */ water_task_handle);
    }

    // Return control to the menu
    return true;
}

bool Context::spray(bool in_isr)
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

    // Return control to the menu
    return true;
}

bool Context::get(
    CONTEXT_MEMBER_t member,
    void *dst,
    size_t num_bytes_dst)
{
    // Take the Context sempahore to assure the member being gotten isn't being modified while it is being copied
    if(pdFALSE == xSemaphoreTake(/* xSemaphore = */ mutex_handle, /* xBlockTime = */ 1000))
    {
        return false;
    }

    // Get the member to copy
    // TODO: Some members don't need to grab the mutex because they are constant after initialization
    //       A hashmap may be able to go faster, just up to me if I want to use the memory to store it
    void *src = nullptr;
    size_t num_bytes_src = 0;
    switch(member)
    {
        case CONTEXT_MEMBER_MUTEX_HANDLE:
            src = &mutex_handle;
            num_bytes_src = sizeof(mutex_handle);
            break;
        case CONTEXT_MEMBER_SERVO:
            src = &servo;
            num_bytes_src = sizeof(servo);
            break;
        case CONTEXT_MEMBER_ROTATE_SERVO_TASK_HANDLE:
            src = &rotate_servo_task_handle;
            num_bytes_src = sizeof(rotate_servo_task_handle);
            break;
        case CONTEXT_MEMBER_PIN_SOIL_MOISTURE_SENSOR_IN:
            src = &pin_soil_moisture_sensor_in;
            num_bytes_src = sizeof(pin_soil_moisture_sensor_in);
            break;
        case CONTEXT_MEMBER_WATER_TASK_HANDLE:
            src = &water_task_handle;
            num_bytes_src = sizeof(water_task_handle);
            break;
        case CONTEXT_MEMBER_PERCENT_CURRENT_HUMIDITY:
            src = &percent_current_humidity;
            num_bytes_src = sizeof(percent_current_humidity);
            break;
        case CONTEXT_MEMBER_PERCENT_DESIRED_HUMIDITY:
            src = &percent_desired_humidity;
            num_bytes_src = sizeof(percent_desired_humidity);
            break;
        case CONTEXT_MEMBER_MINUTE_HUMIDITY_CHECK_FREQ:
            src = &minute_humidity_check_freq;
            num_bytes_src = sizeof(minute_humidity_check_freq);
            break;
        case CONTEXT_MEMBER_TIME_LAST_HUMIDITY_CHECK:
            src = &time_last_humidity_check;
            num_bytes_src = sizeof(time_last_humidity_check);
            break;
        case CONTEXT_MEMBER_TIME_NEXT_HUMIDITY_CHECK:
            src = &time_next_humidity_check;
            num_bytes_src = sizeof(time_next_humidity_check);
            break;
        default:
            return false;
    }

    // Copy the desired member to the passed in buffer
    // This is just a basic memcpy_s, but implemented by hand beause it's easy to do,
    // and I don't want to import the library
    // TODO: Can I just use memcpy with min?
    bool status = false;
    if(num_bytes_src >= num_bytes_dst)
    {
        status = true;
        const uint8_t *s_end = ((uint8_t *) src) + num_bytes_src;
        for(uint8_t *d = (uint8_t *) dst, *s = (uint8_t *) src; s < s_end; ++d, ++s)
        {
           *d = *s;
        }
    }

    // Return mutex so other threads can read and write to context emmbers again
    xSemaphoreGive(/* xSemaphore = */ mutex_handle);

    return status;
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