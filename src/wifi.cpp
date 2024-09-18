// Resources:
// - Create a basic POSIX TPC/IP server in C:
//   https://medium.com/@coderx_15963/basic-tcp-ip-networking-in-c-using-posix-9a074d65bb35
// - How to connect your ESP32 to Wifi and an existing POSIX TCP/IP server:
//   https://www.youtube.com/watch?v=_dRrarmQiAM
//   https://github.com/espressif/esp-idf/blob/v5.3.1/examples/wifi/getting_started/station/main/station_example_main.c
//   https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif.html
//   https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_event.html
//   https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html

// Include custom WiFi API
#include "wifi.h"

#if WIFI_ENABLED

// Include FreeRTOS common header
#include "freertos/FreeRTOS.h"
// Include FreeRTOS event group API
#include "freertos/event_groups.h"
// Include ESP WiFi API
#include "esp_wifi.h"
// Include ESP event API
#include "esp_event.h"

// ======================= //
// Define useful constants //
// ======================= //

// Define bitmask for success in WiFi event loop
#define WIFI_CONNECTED_BIT BIT0
// Define bitmask for failure in WiFi event loop
#define WIFI_FAIL_BIT BIT1
// Define the max number of retries allowed to try to connect to one AP in a row
#define NUM_MAX_WIFI_CONNECT_RETRIES 5

// ======================= //
// Instantiate useful data //
// ======================= //

// For commenting purposes:
// - R = read
// - W = write

// ------------------------------------------------- //
// Intended for sequential access, no mutex required //
// ------------------------------------------------- //

// Keep track of the handle to the WiFi event group, so we can set success and failure bits on its status
// R | W | function
// --+---+------------------
// X | - | event_any_wifi
// X | - | event_got_ip
// X | X | wifi_event_group_init
// X | X | wifi_event_group_free
// X | - | wifi_connect
// Don't need a mutex if only one thread is calling the WiFi APIs at a time,
// the only of these functions that may run in parallel (event_*) are read-only.
// The rest of the functions are static (accessible from this file only)
// and called sequentially by only wifi_start(...).
// TODO: Make a mutex for accessing this API
EventGroupHandle_t event_group_handle_wifi = nullptr;

// ------------------------------------------------- //
// Intended for multithreaded access, mutex required //
// ------------------------------------------------- //

// Keep track of how many times the device has retried connecting to one AP in a row
// R | W | function
// --+---+------------------
// X | X | event_any_wifi
// - | X | event_got_ip
// TODO: should this variable have a mutex?
size_t num_wifi_connect_retries = 0;
// Keep track of the esp_netif object attaching netif to WiFi and registering WiFi handlers to the default event loop
// R | W | function
// --+---+------------------
//   |   | 
//   |   | 
// TODO: should this variable have a mutex?
void *esp_netif = nullptr;

// ============================== //
// Define code to connect to WiFi //
// ============================== //

static inline bool wifi_init();
// bool wifi_free();
static void event_any_wifi(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data);
static void event_got_ip(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data);
static inline bool wifi_event_group_init(
    esp_event_handler_instance_t &event_handler_instance_wifi,
    esp_event_handler_instance_t &event_handler_instance_ip);
static bool wifi_event_group_free(
    esp_event_handler_instance_t &event_handler_instance_wifi,
    esp_event_handler_instance_t &event_handler_instance_ip);
static inline bool wifi_connect(
    char *wifi_ssid,
    char *wifi_password);

// If all went well, by the end of this function, the device will have IP address
// and can connect to a socket to talk with other servers on a network
bool wifi_start(
    char *wifi_ssid,
    char *wifi_password)
{
    // Allocate TCP/IP/WiFi resources
    if(false == wifi_init())
    {
        s_println("Failed to initialize TCP/IP/WiFi resources");
        (void) wifi_free();
        return false;
    }

    // Create event loop, connect to AP
    esp_event_handler_instance_t event_handler_instance_wifi = nullptr;
    esp_event_handler_instance_t event_handler_instance_ip = nullptr;
    if(false == wifi_event_group_init(
        /* esp_event_handler_instance_t &event_handler_instance_wifi = */ event_handler_instance_wifi,
        /* esp_event_handler_instance_t &event_handler_instance_ip = */ event_handler_instance_ip))
    {
        s_println("Failed to create WiFi event group");
        (void) wifi_event_group_free(
            /* esp_event_handler_instance_t &event_handler_instance_wifi = */ event_handler_instance_wifi,
            /* esp_event_handler_instance_t &event_handler_instance_ip = */ event_handler_instance_ip);
        (void) wifi_free();
        return false;
    }

    // Connect to AP
    if(false == wifi_connect(
        /* char *wifi_ssid = */ wifi_ssid,
        /* char *wifi_password = */ wifi_password))
    {
        s_print("Failed to connect to AP: ");
        s_println(wifi_ssid);
        (void) wifi_event_group_free(
            /* esp_event_handler_instance_t &event_handler_instance_wifi = */ event_handler_instance_wifi,
            /* esp_event_handler_instance_t &event_handler_instance_ip = */ event_handler_instance_ip);
        (void) wifi_free();
        return false;
    }

    // Don't need IP and WiFi event handlers anymore, or event loop, can free them
    (void) wifi_event_group_free(
        /* esp_event_handler_instance_t &event_handler_instance_wifi = */ event_handler_instance_wifi,
        /* esp_event_handler_instance_t &event_handler_instance_ip = */ event_handler_instance_ip);
    
    // Everything went well, return success
    s_print("Connected to AP: ");
    s_println(wifi_ssid);
    return true;
}

#if 0
inline void find_ap()
{
    // Scan for available wifi networks
    ESP_ERROR_CHECK(esp_wifi_scan_start(
        // configuration settings for scanning, if set to NULL default settings will be used of
        // which default values are show_hidden:false, scan_type:active, scan_time.active.min:0,
        // scan_time.active.max:120 milliseconds, scan_time.passive:360 milliseconds
        // home_chan_dwell_time:30ms
        /* const wifi_scan_config_t *config = */ NULL,
        // if block is true, this API will block the caller until the scan is done, otherwise it will return immediately
        /* bool block = */ true));

    // Stop if it takes longer than 10 seconds
    vTaskDelay(NULL, 10,000 ms);
    ESP_ERROR_CHECK(esp_wifi_scan_stop());

    // Get the number of AP found
    uint16_t num_ap_found = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(/* uint16_t *number = */ num_ap_found));
    wifi_ap_record_t *ap_record = new wifi_ap_record[num_ap_found];

    // Get record of all APs found (getting removes APs)
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(
	/* uint16_t *number = */ num_ap_found,
        /* wifi_ap_record_t *ap_records = */ ap_records));
    delete [] ap_record;

    // Free memory storing last AP search results
    ESP_ERROR_CHECK(esp_wifi_clear_ap_list());

    // Get AP currently connected to (NOTE: shouldn't abort on fail, WHO is it connecting to???) 
    ESP_ERROR_CHECK(esp_wifi_sta_get_ap_info(/* wifi_ap_record_t *ap_info = */ &ap_info));

    // Set MAC address of this station (this can only be caled when the intterface is disabled?)
    // ESP_ERROR_CHECK(esp_wifi_set_mac(
    //    /* wifi_interface_t ifx = */,
    //    /* const uint8_t mac[6] = */);

    // Get MAC address of this station
    ESP_ERROR_CHECK(esp_wifi_get_mac(
        /* wifi_interface_t ifx = */ ,
        /* uint8_t mac[6] = */ mac_addr);
}
#endif

// Allocate resources needed for TCP/IP/WiFi
static inline bool wifi_init()
{
    // Save status from checks
    // If any check fails, do none of the following
    esp_err_t status = ESP_OK;

    // Initialize the underlying TCP/IP stack
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_netif_init());

    // Initialize the default ESP event loop
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_event_loop_create_default());

    // Create default WiFi station (something that connects to a router,
    // instead of serving as a router itself) in the WiFi driver
    esp_netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, (nullptr != esp_netif) ? ESP_OK : ESP_FAIL);

    // Initialize and allocate WiFi structures and start its task with default settings,
    // We can worry about getting the correct credentials and authentication method later
    const wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_wifi_init(/* const wifi_init_config_t *config = */ &wifi_init_config));

    return true;
}

// Free resources needed for TCP/IP/WiFi
bool wifi_free()
{
    // Save status from checks
    // If any check fails, still do all the following
    esp_err_t status = ESP_OK;
    bool return_status = true;

    // Stop WiFi
    ESP_ERROR_RECORD_FALSE_IF_FAILED(return_status, status, esp_wifi_stop());

    // Dealloc WiFi stack and task
    // TODO: Look into WiFi low-power mode, for example: esp_wifi_set_ps(/* wifi_ps_type_t type = */ ...)
    ESP_ERROR_RECORD_FALSE_IF_FAILED(return_status, status, esp_wifi_deinit());

    // Dealloc WiFi station
    esp_netif_destroy_default_wifi(/* void *esp_netif = */ esp_netif);

    // Dealloc default event loop
    ESP_ERROR_RECORD_FALSE_IF_FAILED(return_status, status, esp_event_loop_delete_default());

    // Dealloc underlying TCP/IP stack
    ESP_ERROR_RECORD_FALSE_IF_FAILED(return_status, status, esp_netif_deinit());

    return return_status;
}

// This event handles all WiFi events (although it only acts on START and DISCONNECTED)
static void event_any_wifi(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    // Ignore non-WiFi events
    if (WIFI_EVENT != event_base)
    {
        return;
    }

    // Based on kind of event, do different actions
    switch(event_id)
    {
        // esp_wifi_start(...) returned ESP_OK and the current WiFi mode is at least part station
        case WIFI_EVENT_STA_START:
            // Connect to WiFi for the first time
            s_println("Connecting to AP");
            (void) esp_wifi_connect();
            break;

        // esp_wifi_disconnect(...) or esp_wifi_stop(...) was called while the station was connected to an AP,
        // esp_wifi_connect(...) was called but the WiFi driver failed connection setup, or
        // the WiFi connection was disrupted
        case WIFI_EVENT_STA_DISCONNECTED:
            // We are still under our max-retry threshold
            if(num_wifi_connect_retries < NUM_MAX_WIFI_CONNECT_RETRIES)
            {
                // Retry connecting to WiFi
                s_print("Reconnecting to AP (attempt ");
                s_print(num_wifi_connect_retries, DEC);
                s_println(")");
                (void) esp_wifi_connect();
                ++num_wifi_connect_retries;
                break;
            }

            // We have exceeded our max-retry threshold
            s_print("Could not connect to AP with max retries (");
            s_print(NUM_MAX_WIFI_CONNECT_RETRIES, DEC);
            s_println("). Marking connection as having failed.");
            // Set bits for WiFi event group, unblocking tasks waiting for bits, indicating failure
            (void) xEventGroupSetBits(
                /* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi,
                /* const EventBits_t uxBitsToSet = */ WIFI_FAIL_BIT);
            break;

        // Another WiFi event we don't care about has happened
        default:
            s_print("Ignoring WiFi event: ");
            s_println(event_id, DEC);
            break;
    }
}

// This event handles got IP events
static void event_got_ip(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    // Ignore non-IP events
    if (IP_EVENT != event_base)
    {
        return;
    }

    // Based on kind of event, do different actions
    switch(event_id)
    {
        // This device's WiFi station got IP from connected AP
        case IP_EVENT_STA_GOT_IP:
            // Need scope to be able to create local variable
            s_print("Got station IP address: ");
            {
                ip_event_got_ip_t *got_ip_event = (ip_event_got_ip_t *) event_data;
                // TODO: Use IP2STR instead, it's just incompatible with Arduino print
                s_print(esp_ip4_addr1_16(&(got_ip_event->ip_info.ip)), DEC);
                s_print(".");
                s_print(esp_ip4_addr2_16(&(got_ip_event->ip_info.ip)), DEC);
                s_print(".");
                s_print(esp_ip4_addr3_16(&(got_ip_event->ip_info.ip)), DEC);
                s_print(".");
                s_println(esp_ip4_addr4_16(&(got_ip_event->ip_info.ip)), DEC);
            }
            num_wifi_connect_retries = 0;
            // Set bits for WiFi event group, unblocking tasks waiting for bits, indicating success
            (void) xEventGroupSetBits(
                /* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi,
                /* const EventBits_t uxBitsToSet = */ WIFI_CONNECTED_BIT);
            break;

        // Another IP event we don't care about has happened
        default:
            s_print("Ignoring IP event: ");
            s_println(event_id, DEC);
            break;
    }
}

// Allocate event group in main event loop (which will be connected to WiFi)
static inline bool wifi_event_group_init(
    esp_event_handler_instance_t &event_handler_instance_wifi,
    esp_event_handler_instance_t &event_handler_instance_ip)
{
    // Save status from checks
    // If any check fails, do none of the following
    esp_err_t status = ESP_OK;

    // TODO: read more about events to understand them, static version exists
    // Create a new event group, something like a queue that holds what asynchronous events
    // have happened, an inifinte loop checks it, and calls their corresponding event handler
    // (callback function), if they have one
    event_group_handle_wifi = xEventGroupCreate();
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, (nullptr != event_group_handle_wifi) ? ESP_OK : ESP_FAIL);

    // Register a new instance of an event handler to the event loop to handle WiFi
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_event_handler_instance_register(
        // the base ID of the event to register the handler for
        /* esp_event_base_t event_base = */ WIFI_EVENT,
        // the ID of the event to register the handler for
        //     ESP_EVENT_ANY_IF = all WiFi events should call this function
        /* int32_t event_id = */ ESP_EVENT_ANY_ID,
        // the handler function which gets called when the event is dispatched
        /* esp_event_handler_t event_handler = */ &event_any_wifi,
        // data, aside from event data, that is passed to the handler when it is called
        /* void *event_handler_arg = */ NULL,
        // An event handler instance object related to the registered event handler and data, can be NULL.
        // This needs to be kept if the specific callback instance should be unregistered before deleting
        // the whole event loop. Registering the same event handler multiple times is possible and yields
        // distinct instance objects. The data can be the same for all registrations. If no unregistration
        // is needed, but the handler should be deleted when the event loop is deleted, instance can be NULL.
        /* esp_event_handler_instance_t *instance = */ &event_handler_instance_wifi));

    // Register a new instance of an event handler to the event loop to handle getting IP (internet protocol) packets
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_event_handler_instance_register(
        // the base ID of the event to register the handler for
        /* esp_event_base_t event_base = */ IP_EVENT,
        // the ID of the event to register the handler for
        //     IP_EVENT_STA_GOT_IP = when I get a got IP event specifically, call this function
        /* int32_t event_id = */ IP_EVENT_STA_GOT_IP,
        // the handler function which gets called when the event is dispatched
        /* esp_event_handler_t event_handler = */ &event_got_ip,
        // data, aside from event data, that is passed to the handler when it is called
        /* void *event_handler_arg = */ NULL,
        // An event handler instance object related to the registered event handler and data, can be NULL.
        // This needs to be kept if the specific callback instance should be unregistered before deleting
        // the whole event loop. Registering the same event handler multiple times is possible and yields
        // distinct instance objects. The data can be the same for all registrations. If no unregistration
        // is needed, but the handler should be deleted when the event loop is deleted, instance can be NULL.
        /* esp_event_handler_instance_t *instance = */ &event_handler_instance_ip));

    return true;
}

// Free event group in main event loop (which is connected to WiFi)
static bool wifi_event_group_free(
    esp_event_handler_instance_t &event_handler_instance_wifi,
    esp_event_handler_instance_t &event_handler_instance_ip)
{
    // Save status from checks
    // If any check fails, still do all the following
    esp_err_t status = ESP_OK;
    bool return_status = true;

    // Don't need IP and WiFi event handlers anymore, can free them
    // Free IP event handler
    ESP_ERROR_RECORD_FALSE_IF_FAILED(return_status, status, esp_event_handler_instance_unregister(
        /* esp_event_base_t event_base = */ IP_EVENT,
        /* int32_t event_id = */ IP_EVENT_STA_GOT_IP,
        /* esp_event_handler_instance_t instance = */ event_handler_instance_ip));

    // Free WiFi event handler
    ESP_ERROR_RECORD_FALSE_IF_FAILED(return_status, status, esp_event_handler_instance_unregister(
        /* esp_event_base_t event_base = */ WIFI_EVENT,
        /* int32_t event_id = */ ESP_EVENT_ANY_ID,
        /* esp_event_handler_instance_t instance = */ event_handler_instance_wifi));

    // Free WiFi event group
    vEventGroupDelete(/* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi);
    event_group_handle_wifi = nullptr;

    return return_status;
}

// Connect to AP
static inline bool wifi_connect(
    char *wifi_ssid,
    char *wifi_password)
{
    // Save status from checks
    // If any check fails, do none of the following
    esp_err_t status = ESP_OK;

    // Set this ESP32 to a station, something that connects to a router,
    // instead of an AP (access point), something like a router
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_wifi_set_mode(/* wifi_mode_t mode = */ WIFI_MODE_STA));

    // Set configuration of this station and AP to connect to
    // NOTE: we can save this in nvs_flash (non-volatile storage, to remember between reboots) if we want
    // So many workarounds just to initialize this in C++...
    wifi_config_t wifi_config = { 0 };
    (void) memcpy(
        /* void* dest = */ wifi_config.sta.ssid,
        /* const void* src = */ wifi_ssid,
        /* std::size_t count = */ min(sizeof(wifi_config.sta.ssid), strlen(wifi_ssid)));
    (void) memcpy(
        /* void* dest = */ wifi_config.sta.password,
        /* const void* src = */ wifi_password,
        /* std::size_t count = */ min(sizeof(wifi_config.sta.ssid), strlen(wifi_password)));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = true;
    // TODO: Should I set these from the example?
    //    .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
    //    .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
    //    .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,

    // Configure WiFi for this device as station
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_wifi_set_config(
        /* wifi_interface_t interface = */ WIFI_IF_STA,
        /* wifi_config_t *conf = */ &wifi_config));

    // Start Wifi with current configuration, creating control block for mode (STA, AP, STA & AP),
    // kicking off event loop for wifi controller
    ESP_ERROR_RETURN_FALSE_IF_FAILED(status, esp_wifi_start());

    s_println("WiFi event loop started, waiting to connect");

    // Block this function for 5 seconds or until we get a WiFi success or failure
    const EventBits_t event_bits = xEventGroupWaitBits(
        /* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi,
        /* const EventBits_t uxBitsToWaitFor = */ WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        /* const BaseType_t xClearOnExit = */ pdFALSE,
        /* const BaseType_t xWaitForAllBits = */ pdFALSE,
        /* TickType_t xTicksToWait = */ pdMS_TO_TICKS(5000));

    return (event_bits & WIFI_CONNECTED_BIT) > 0;
}

#endif // WIFI_ENABLED