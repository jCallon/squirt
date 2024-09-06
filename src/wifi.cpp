#include "wifi.h"

#if WIFI_ENABLED

// TODO: comment
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"

// TODO: comment
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

// Include WiFi credentials
// TODO: Is there a better way to do this? Do I care? Eventually I may implement just being able to search among many WiFi networks.
#include "credentials.h"
#ifndef WIFI_SSID
#error "Please make a include/credentials.h, and in it, #define WIFI_SSID \"your_wifi_nickname\", then pad with as many \0 as needed."
#endif // WIFI_SSID
#ifndef WIFI_PASSWORD
#error "Please make a include/credentials.h, and in it, #define WIFI_PASSWORD \"your_wifi_password\", then pad with as many \0 as needed."
#endif // WIFI_PASSWORD

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// Resources:
// - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_netif.html
// - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/esp_event.html
// - https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_wifi.html
// - https://github.com/espressif/esp-idf/blob/v5.3.1/examples/wifi/getting_started/station/main/station_example_main.c
// - https://www.youtube.com/watch?v=_dRrarmQiAM

#define NUM_MAX_WIFI_CONNECT_RETRIES 5
size_t num_wifi_connect_retries = 0;
EventGroupHandle_t event_group_handle_wifi = 0;

// Handle WiFi events
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

    switch(event_id)
    {
        case WIFI_EVENT_STA_START:
            // Connect to WiFi for the first time
            s_println("Connecting to AP\n");
            (void) esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if(num_wifi_connect_retries <= NUM_MAX_WIFI_CONNECT_RETRIES)
            {
                // Try to connect to WiFi again
                s_println("Reconnecting to AP\n");
                (void) esp_wifi_connect();
                ++num_wifi_connect_retries;
            }
            else
            {
                // Set bits for WiFi event group, unblocking tasks waiting for bits
                (void) xEventGroupSetBits(
                    /* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi,
                    /* const EventBits_t uxBitsToSet = */ WIFI_FAIL_BIT);
            }
            break;
        default:
            s_print("Ignoring WiFi event: ");
            s_println(event_id, DEC);
            break;
    }
}

// Handle got IP events
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

    switch(event_id)
    {
        case IP_EVENT_STA_GOT_IP:
            s_print("Got STA IP: ");
            //s_println(IP2STR(&(((ip_event_got_ip_t *) event_data)->ip_info.ip)));
            num_wifi_connect_retries = 0;
            // Set bits for IP event group, unblocking tasks waiting for bits
            (void) xEventGroupSetBits(
                /* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi,
                /* const EventBits_t uxBitsToSet = */ WIFI_CONNECTED_BIT);
            break;
        default:
            s_print("Ignoring IP event: ");
            s_println(event_id, DEC);
            break;
    }
}

// If all went well, by the end you will have IP address,
// and can connect to a socket to talk with other servers on a network
bool connect_wifi()
{
    // Initialize the underlying TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize the default ESP event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create default WiFi station (something that connects to a router,
    // instead of serving as a router itself) in the WiFi driver
    const esp_netif_t *esp_netif = esp_netif_create_default_wifi_sta();
    configASSERT(esp_netif);

    // Initialize and allocate WiFi structures and start its task with default settings,
    // We can worry about getting the correct credentials and authentication method later
    const wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(/* const wifi_init_config_t *config = */ &wifi_init_config));

    // TODO: read more about events to understand them, static version exists
    // Create a new event group, something like a queue that holds what asynchronous events
    // have happened, an inifinte loop checks it, and calls their corresponding event handler
    // (callback function), if they have one
    event_group_handle_wifi = xEventGroupCreate();
    configASSERT(event_group_handle_wifi);

    // Register a new instance of an event handler to the event loop to handle WiFi
    esp_event_handler_instance_t event_handler_instance_wifi;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
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
    esp_event_handler_instance_t event_handler_instance_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
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

    // Set this ESP32 to a station, something that connects to a router,
    // instead of an AP (access point), something like a router
    ESP_ERROR_CHECK(esp_wifi_set_mode(/* wifi_mode_t mode = */ WIFI_MODE_STA));

    // Set configuration of this station and AP to connect to
    // NOTE: we can save this in nvs_flash (non-volatile storage, to remember between reboots) if we want
    // So many workarounds just to initialize this in C++...
    wifi_config_t wifi_config = { 0 };
    (void) memcpy(
        /* void* dest = */ wifi_config.sta.ssid,
        /* const void* src = */ WIFI_SSID,
        /* std::size_t count = */ min(sizeof(wifi_config.sta.ssid), sizeof(WIFI_SSID)));
    (void) memcpy(
        /* void* dest = */ wifi_config.sta.password,
        /* const void* src = */ WIFI_PASSWORD,
        /* std::size_t count = */ min(sizeof(wifi_config.sta.ssid), sizeof(WIFI_PASSWORD)));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = true;
    // TODO: Should I set these from the example?
    //    .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
    //    .sae_pwe_h2e = ESP_WIFI_SAE_MODE,
    //    .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,

    ESP_ERROR_CHECK(esp_wifi_set_config(
        /* wifi_interface_t interface = */ WIFI_IF_STA,
        /* wifi_config_t *conf = */ &wifi_config));

    // Start Wifi with current configuration, creating control block for mode (STA, AP, STA & AP),
    // kicking off event loop for wifi controller
    ESP_ERROR_CHECK(esp_wifi_start());

    s_println("WiFi event loop started, waiting to connect");

    // Block this function for 5 seconds or until we get a WiFi success or failure
    const EventBits_t event_bits = xEventGroupWaitBits(
        /* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi,
        /* const EventBits_t uxBitsToWaitFor = */ WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        /* const BaseType_t xClearOnExit = */ pdFALSE,
        /* const BaseType_t xWaitForAllBits = */ pdFALSE,
        /* TickType_t xTicksToWait = */ pdMS_TO_TICKS(5000));

    // Don't need IP and WiFi event handlers anymore, can free them
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
        /* esp_event_base_t event_base = */ IP_EVENT,
        /* int32_t event_id = */ IP_EVENT_STA_GOT_IP,
        /* esp_event_handler_instance_t instance = */ event_handler_instance_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
        /* esp_event_base_t event_base = */ WIFI_EVENT,
        /* int32_t event_id = */ ESP_EVENT_ANY_ID,
        /* esp_event_handler_instance_t instance = */ event_handler_instance_wifi));
    vEventGroupDelete(/* EventGroupHandle_t xEventGroup = */ event_group_handle_wifi);
    
    // Return result of overall operation
    return (bool) (event_bits & WIFI_CONNECTED_BIT);

#if 0
    // Support 2.3G or 5G, I don't personally care about this or the bandwidth, and i don't kn0ow what channel means, promiscuous, vendor ie, disabling or enabling certain kinds of events, configuring an antenna lol, rssi,ftm,bitrte
    // ESP_ERROR_CHECK(esp_wifi_set_protocol(wifi_interface_t ifx, uint8_t protocol_bitmap));

    // Connect to a wifi network
    ESP_ERROR_CHECK(esp_wifi_connect());

    // Register the RX (reception) callback function of CSI (channel state information) data. 
    ESP_ERROR_CHECK(esp_err_t esp_wifi_set_csi(/* bool en = */ true));
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(
        /* wifi_csi_cb_t cb = */ ,
        /* void *ctx = */ ));

    // debug: esp_err_t esp_wifi_statis_dump(uint32_t modules)
#endif
}

// Connect and read IPv4 data
// If you want to connect to IPv6 or use another protocol, please update this function
bool connect_tcp_server()
{
    // Instantiate server information and a blank buffer to store reads in
    struct sockaddr_in server_info = { 0 };
    char read_buffer[1024] = { 0 };

    // Give the protocol family that will be used for this server
    server_info.sin_family = AF_INET;
    // Set server address to some hard-coded IP address
    // TODO: where does this hard-coded value come from?
    server_info.sin_addr.s_addr = 0x8200140a;
    // Set the port by converting some hard-coded value to from host-byte-order to network byte order
    // TODO: where does this hard-coded value come from?
    // https://linux.die.net/man/3/htons
    server_info.sin_port = htons(/* uint32_t hostlong = */ 12345);
    
    // Get a file descriptor to use as an endpoint for communcation
    // https://www.man7.org/linux/man-pages/man2/socket.2.html
    const int file_descriptor = socket(
        // The protocol family which will be used for communication
        // ex: AF_INET = IPv4 Internet protocols
        //     AF_INET6 = IPv6 Internet protocols
        //     AF_BLUETOOTH = Bluetooth low-level socket protocol
        /* int domain = */ AF_INET,
        // The communication semantics
        // ex. SOCK_STREAM = Sequenced, reliable, two-way, connection-based byte streams. Out-of-band data transmission mechanism.
        //     SOCK_SEQPACKET = Sequenced, reliable, two-way connection-based data transmission path for datagrams of fixed maximum length.
        //                      A consumer is required to read an entire packet with each input system call. Not always supported.
        /* int type = */ SOCK_STREAM,
        // What protocol to used with the socket. 
        // Normally only a single protocol exists to support a particular socket type within a given protocol family,
        // in which case protocol can be specified as 0.
        /* int protocol = */ 0);
    if (0 > file_descriptor)
    {
        s_println("Failed creating TCP socket");
        return false;
    }

    // Connect to the server, whose IP address and port is given to us by server_info
    // https://man7.org/linux/man-pages/man2/connect.2.html
    // https://linux.die.net/man/3/inet_ntoa
    // https://man7.org/linux/man-pages/man2/close.2.html
    if (0 != connect(
        /* int sockfd = */ file_descriptor,
        /* const struct sockaddr *addr = */ (struct sockaddr *) &server_info,
        /* socklen_t addrlen = */ sizeof(server_info)))
    {
        s_print("Failed to connect to: ");
        s_println(inet_ntoa(/* struct in_addr in = */ server_info.sin_addr.s_addr));
        close(/* int fd = */ file_descriptor);
        return false;
    }

    s_println("Connected to TCP server");

    // Get the number of bytes in the file descriptor,
    // copy up to count bytes from the file descriptor to our read buffer
    // https://man7.org/linux/man-pages/man2/read.2.html
    const int num_read_bytes = read(
        /* int fd = */ file_descriptor,
        /* void buf[.count] = */ read_buffer,
        /* size_t count = */ sizeof(read_buffer) - 1);

    // Print the bytes from our user input to stdout
    for(size_t i = 0; i < num_read_bytes; ++i)
    {
        // https://man7.org/linux/man-pages/man3/putchar.3p.html
        putchar(/* int c = */ read_buffer[i]);
    }

    // Use this to react to user input
    // TODO: make it something useful
    const char example[] = "Hello world!";
    if(strncmp(read_buffer, example, min(sizeof(read_buffer), sizeof(example))))
    {
        s_println("Hello world!");
    }

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

inline void stop_wifi()
{
    // Remove WiFi driver from system (there's also a function to just disconnect, could use that instead)
    ESP_ERROR_CHECK(esp_wifi_deinit());

    // Set WiFi power save type
    // ESP_ERROR_CHECK(esp_wifi_set_ps(/* wifi_ps_type_t type = */ ));
}
#endif

#endif // WIFI_ENABLED