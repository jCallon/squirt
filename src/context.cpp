// Include custom Context class implementation
#include "context.h"
// Include custom debug macros and compile flags
#include "flags.h"

// ======================================= //
// Define reusable tasks, interrupts, etc. //
// ======================================= //

void task_rotate_servo(Servo *servo)
{
    // I've found with the API I'm using, if the servo is not written to first, its first read value will be garbage
    servo->write(/* int value = */ 0);

    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    // TODO: This seems to turn a bit more than 90 degrees, calibrate it using the library?
    const int angles[] = {0, 90, 0};
    const size_t num_angle = sizeof(angles) / sizeof(*angles);
    size_t angles_i = 0;
    int angle_delta = 0;
    while(1)
    {
        // Suspend this task until it is needed
        vTaskSuspend(/* TaskHandle_t xTaskToSuspend = */ NULL);

        // For every angle we want to reach...
        for(angles_i = 0; angles_i < num_angle; ++angles_i)
        {
            // If the angle is already at what we want, don't need to do anything
            angle_delta = abs(servo->read() - angles[angles_i]);
            if(0 == angle_delta)
            {
                continue;
            }

            // Tell the servo to go to a certain angle, wait until it reaches it or timeout
            // NOTE: angle_delta is used as a timeout here, and assumes 1 second = 100 degrees in an ideal case.
            //       It gives 2x that amount of time to be lenient to bad cases.
            //       100ms = .1s .1s * 100deg = 10deg, 10deg / 2 = 5deg
            servo->write(/* int value = */ angles[angles_i]);
            do
            {
                vTaskDelay(/* const TickType_t xTicksToDelay = */ pdMS_TO_TICKS(100));
                angle_delta -= 5;
            } while ((angle_delta > 0) && (servo->read() != angles[angles_i]));
        }

        // 29OCT2024: usStackDepth = 1024, uxTaskGetHighWaterMark = 252
        PRINT_STACK_USAGE();
    }
}

void task_water(Context *context)
{
    // Tasks must be implemented to never return (i.e. continuous loop)
    // https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/freertos_idf.html
    // Read the soil moisture sensor, while the soil moisture is below our desired threshold, add more water
    while(1)
    {
        // Wait until it is the time for the next moisture check
        // TODO: Is there a more efficient way to do this?
        //       Maybe have a timer task that simply resumes or notifies this task?
        while(false == context->is_soil_moisture_check_overdue())
        {
            // Wait 1 minute
            vTaskDelay(/* TickType_t xTicksToDelay = */ 60 * 1000);
        }

        // Update context's current moisture, time last checked, and time of next check
        // if we fail, oh well, not really worth waiting until it works
        (void) context->check_soil_moisture(/* bool update_next_moisture_check = */ true);

        // While we are not at our desired moisture, add more water
        // TODO: Optimize the number of sensor polls needed.
        //       1. Do, say, 10 squirts. See how much it increases the soil moisture, find the average from that sample size.
        //       2. Poll soil moisture
        //       3. Use the earlier average to calculate how many squirts are needed. Do that many squirts.
        //       4. Repeat from 2. This reduces the number of times the sensor needs to be polled, reducing corrosion.
        while(true == context->is_current_soil_moisture_below_desired())
        {
            // Trigger servo motor to squirt, wait until it finishes
            (void) context->spray(
                /* bool in_isr = */ false,
                /* bool is_blocking = */ true);

            // Wait for water from squirting to soak into soil
            vTaskDelay(/* TickType_t xTicksToDelay = */ pdMS_TO_TICKS(5000));

            // Update context's current moisture, time last checked, and time of next check
            // if we fail, oh well, not really worth waiting until it works
            (void) context->check_soil_moisture(/* bool update_next_moisture_check = */ true);
        }

        // 24OCT2024: usStackDepth = 2048, uxTaskGetHighWaterMark = 1232
        // TODO: Update this for the "Add NVS" PR.
        PRINT_STACK_USAGE();
    }
}

// ======================== //
// Context member functions //
// ======================== //

Context::Context(
    StaticSemaphore_t *arg_mutex_buffer,
    int arg_pin_servo_out,
    gpio_num_t arg_pin_soil_moisture_sensor_in,
    char *arg_nvs_namespace)
{
    // Create mutex, open it for grabbing
    // NOTE: "Mutex type semaphores cannot be used from within interrupt service routines."
    mutex_handle = xSemaphoreCreateMutexStatic(/* pxMutexBuffer = */ arg_mutex_buffer);
    assert(nullptr != mutex_handle);

    // Attach servo
    servo.attach(/* int pin = */ arg_pin_servo_out);

    // Remember the pin of the soil moisture sensor
    pin_soil_moisture_sensor_in = arg_pin_soil_moisture_sensor_in;

    // Set the ADC attenuation to 11 dB (up to ~3.3V input)
    // https://esp32io.com/tutorials/esp32-soil-moisture-sensor
    // TODO: What is this and how do I un-Arduino it?
    analogSetAttenuation(ADC_11db);

    // Get a handle to the NVS namespace for this context
    nvs_namespace = arg_nvs_namespace;
    nvs_handle = 0;
    (void) storage_init(/* bool reinit = */ false);
    (void) storage_open(/* char *name = */ nvs_namespace,
        /* nvs_handle_t *nvs_handle = */ &nvs_handle);

    // Get minute_moisture_check_freq from NVS
    if(false == storage_get(
        /* nvs_handle_t nvs_handle = */ nvs_handle,
        /* char *key = */ CONTEXT_NVS_KEY_MINUTE_SOIL_MOISTURE_CHECK_FREQ,
        /* void *value = */ &minute_soil_moisture_check_freq,
        /* size_t num_value_bytes = */ sizeof(minute_soil_moisture_check_freq)))
    {
        // Set the default moisture check frequency as 1 hour
        minute_soil_moisture_check_freq = 60;
        (void) storage_set(
            /* nvs_handle_t nvs_handle = */ nvs_handle,
            /* char *key = */ CONTEXT_NVS_KEY_MINUTE_SOIL_MOISTURE_CHECK_FREQ,
            /* void *value = */ &minute_soil_moisture_check_freq,
            /* size_t num_value_bytes = */ sizeof(minute_soil_moisture_check_freq));
    }

    // Get the current soil moisture
    // TODO: is this being called before esp_timer_early_init?
    check_soil_moisture(/* bool move_time_next_moisture_check = */ true);

    // Get desired_soil_moisture from NVS
    if(false == storage_get(
        /* nvs_handle_t nvs_handle = */ nvs_handle,
        /* char *key = */ CONTEXT_NVS_KEY_DESIRED_SOIL_MOISTURE,
        /* void *value = */ &desired_soil_moisture,
        /* size_t num_value_bytes = */ sizeof(desired_soil_moisture)))
    {
        // Set the default desired soil moisture as the current soil moisture
        desired_soil_moisture = current_soil_moisture;
        (void) storage_set(
            /* nvs_handle_t nvs_handle = */ nvs_handle,
            /* char *key = */ CONTEXT_NVS_KEY_DESIRED_SOIL_MOISTURE,
            /* void *value = */ &desired_soil_moisture,
            /* size_t num_value_bytes = */ sizeof(desired_soil_moisture));
    }

    // Create task to rotate servo motor (it can take several seconds)
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
}

bool Context::is_soil_moisture_check_overdue()
{
    // If the time of the next check is after the current time, we're overdue
    CONTEXT_LOCK(/* RET_VAL = */ false);
    bool is_overdue = time(/* time_t *_timer = */ nullptr) >= time_next_soil_moisture_check;
    CONTEXT_UNLOCK();

    // Return result of check
    return is_overdue;
}

bool Context::is_current_soil_moisture_below_desired()
{
    // If the current moisture is below the desired moisture, well
    // NOTE: These sensors give a LOWER value when the soil is wetter.
    //       So, a higher value means it is dryer.
    //       The check here is whether the current soil humidity is dryer than what we want.
    CONTEXT_LOCK(/* RET_VAL = */ false);
    bool is_current_below_desired = current_soil_moisture > desired_soil_moisture;
    CONTEXT_UNLOCK();

    // Return result of check
    return is_current_below_desired;
}

MENU_CONTROL Context::check_soil_moisture(bool update_next_moisture_check)
{
    // Get the current soil moisture from the ADC pin,
    // update the time of the last check to now,
    // and update the time of the next check (if desired)
    CONTEXT_LOCK(/* RET_VAL = */ MENU_CONTROL_RELEASE);
    current_soil_moisture = analogRead(pin_soil_moisture_sensor_in);
    time_last_soil_moisture_check = time(/* time_t *_timer = */ nullptr);
    if(update_next_moisture_check)
    {
        // NOTE: time_t is usually represented as seconds since the last epoch
        time_next_soil_moisture_check = time_last_soil_moisture_check + (minute_soil_moisture_check_freq * 60);
    }
    CONTEXT_UNLOCK();

    // Return control to the menu
    return MENU_CONTROL_RELEASE;
}

MENU_CONTROL Context::water()
{
    // Run task_water by updating its wait condition
    CONTEXT_LOCK(/* RET_VAL = */ MENU_CONTROL_RELEASE);
    time_next_soil_moisture_check = time(/* time_t *_timer = */ nullptr);
    CONTEXT_UNLOCK();

    // Return control to the menu
    return MENU_CONTROL_RELEASE;
}

MENU_CONTROL Context::spray(
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
            // TODO: Look into task notifications?
            vTaskDelay(/* TickType_t xTicksToDelay = */ pdMS_TO_TICKS(500));
        }
    }

    // Return control to the menu
    return MENU_CONTROL_RELEASE;
}

MENU_CONTROL Context::set_desired_soil_moisture_to_current()
{
    // Set our desired soil moisture to match the last known soil moisture, in memory and NVS
    CONTEXT_LOCK(/* RET_VAL = */ MENU_CONTROL_RELEASE);
    desired_soil_moisture = current_soil_moisture;
    (void) storage_set(
        /* nvs_handle_t nvs_handle = */ nvs_handle,
        /* char *key = */ CONTEXT_NVS_KEY_DESIRED_SOIL_MOISTURE,
        /* void *value = */ &desired_soil_moisture,
        /* size_t num_value_bytes = */ sizeof(desired_soil_moisture));
    CONTEXT_UNLOCK();

    // Return control to the menu
    return MENU_CONTROL_RELEASE;
}

MENU_CONTROL Context::add_minute_soil_moisture_check_freq(int num_minutes)
{
    // Alter the moisture check frequenecy in memory and NVS
    CONTEXT_LOCK(/* RET_VAL = */ MENU_CONTROL_KEEP);
    minute_soil_moisture_check_freq += num_minutes;
    (void) storage_set(
        /* nvs_handle_t nvs_handle = */ nvs_handle,
        /* char *key = */ CONTEXT_NVS_KEY_MINUTE_SOIL_MOISTURE_CHECK_FREQ,
        /* void *value = */ &minute_soil_moisture_check_freq,
        /* size_t num_value_bytes = */ sizeof(minute_soil_moisture_check_freq));
    CONTEXT_UNLOCK();

    // Do not return control to the menu
    return MENU_CONTROL_KEEP;
}

String Context::str_current_soil_moisture()
{
    // -------------------- //
    //   Current X: 65535   //
    // -------------------- //
    String ret = String("Current X: ");

    CONTEXT_LOCK(/* RET_VAL = */ ret);
    uint16_t cpy = current_soil_moisture;
    CONTEXT_UNLOCK();

    return ret + String(cpy, DEC);
}

String Context::str_desired_soil_moisture()
{
    // -------------------- //
    //   Desired X: 65535   //
    // -------------------- //
    String ret = String("Desired X: ");

    CONTEXT_LOCK(/* RET_VAL = */ ret);
    uint16_t cpy = desired_soil_moisture;
    CONTEXT_UNLOCK();

    return ret + String(cpy, DEC);
}

String Context::str_minute_soil_moisture_check_freq()
{
    // -------------------- //
    //   X freq: 100 min    //
    // -------------------- //
    String ret = String("X freq: ");

    CONTEXT_LOCK(/* RET_VAL = */ ret);
    uint32_t cpy = minute_soil_moisture_check_freq;
    CONTEXT_UNLOCK();

    return ret + String(cpy, DEC) + String(" min");
}

String Context::str_time_last_soil_moisture_check()
{
    // -------------------- //
    //   X read: 999min ago //
    // -------------------- //
    // or
    // -------------------- //
    //   X read: 9999hr ago //
    // -------------------- //
    String ret = String("X read: ");

    CONTEXT_LOCK(/* RET_VAL = */ ret);
    time_t cpy = time_last_soil_moisture_check;
    CONTEXT_UNLOCK();

    // The time difference in minutes is: (current time - time of last check) * (1 minute / 60 seconds)
    uint64_t min_diff = (time(/* time_t *_timer = */ nullptr) - cpy) / 60;

    return ret + ((min_diff < 999) ?
        String(min_diff, DEC) + String("min ago") :
        String(min_diff / 60, DEC) + String("hr ago"));
}

String Context::str_time_next_soil_moisture_check()
{
    // -------------------- //
    //   Next X: in 999min  //
    // -------------------- //
    // or
    // -------------------- //
    //   Next X: in 9999hr  //
    // -------------------- //
    String ret = String("Next X: in ");

    CONTEXT_LOCK(/* RET_VAL = */ ret);
    time_t cpy = time_next_soil_moisture_check;
    CONTEXT_UNLOCK();

    // The time difference in minutes is: (time of next check - current time) * (1 minute / 60 seconds)
    uint64_t min_diff = (cpy - time(/* time_t *_timer = */ nullptr)) / 60;

    return ret + ((min_diff < 999) ?
        String(min_diff, DEC) + String("min") :
        String(min_diff / 60, DEC) + String("hr"));
}