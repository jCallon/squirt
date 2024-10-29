// Include custom TCP/IP API
#include "tcp_ip.h"

#if WIFI_ENABLED

// Include FreeRTOS task API
#include "freertos/task.h"
// Include light-weight IP socket API
#include "lwip/sockets.h"
// Include custom Menu class implementation
#include "menu.h"

// ====================================== //
// Define useful constants and data types //
// ====================================== //

// Define the number of currently supported TCP commands
#define NUM_TCP_COMMANDS 3

// Define, when receiving a TCP packet, what special strings should cause what actions
typedef struct tcp_command_s {
    // The string, that if the TCP packet matches, should trigger an action
    const char *command;
    // The action to be triggered if the TCP packet matched command
    void (*action)();
} tcp_command_t;

// ======================= //
// Instantiate useful data //
// ======================= //

// Intended to be read-only.
// Keep track of the special strings that if a TCP packet matches,
// should execute an action, and what action they should execute
const tcp_command_t tcp_commands[NUM_TCP_COMMANDS] = {
    {
        .command = "up",
        .action = []() { add_to_menu_input_queue(
            /* MENU_INPUT_t menu_input = */ MENU_INPUT_UP,
            /* bool from_isr = */ false); },
    },
    {
        .command = "down",
        .action = []() { add_to_menu_input_queue(
            /* MENU_INPUT_t menu_input = */ MENU_INPUT_DOWN,
            /* bool from_isr = */ false); },
    },
    {
        .command = "confirm",
        .action = []() { add_to_menu_input_queue(
            /* MENU_INPUT_t menu_input = */ MENU_INPUT_CONFIRM,
            /* bool from_isr = */ false); },
    },
#if 0
    {
        .command = "sleep",
        .action = []() { add_to_menu_input_queue(
            /* MENU_INPUT_t menu_input = */ MENU_INPUT_SLEEP,
            /* bool from_isr = */ false); },
    },
#endif
};

// Keep track of the file handle being used as the IP socket connection
int ip_socket_file_descriptor = 0;

// Keep track of the handle of the task that reads IP packets (task_read_ip_packets(...))
TaskHandle_t read_ip_packet_task_handle = nullptr;

// NOTE: For now this code is good enough.
//       Only *sleep*(...), *display*(...), and main(...) call this API.
//       Will need to update this code to be thread-safe if that changes.
//       Ex: keep a vector of open connections instead of only allowing one,
//           represent connections as classes, each with their own locks, data, and methods, etc.,
//           in each connection, only allow one tcp_send() at a time, have a read task, free if connection closes

// ============ //
// Define tasks //
// ============ //

// A task whose job it is to read all incoming TCP packets over the connected IP socket.
// Based on the contents of the TCP packet, it may execute certain actions.
// Once the socket the TCP connection is set up on, ip_socket_file_descriptor,
// is no longer readable, it automatically kills itself and resets the handle.
void task_read_ip_packets()
{
    // Create a 0-initialized buffer IP packets will be read into
    char read_buffer[16] = { 0 };
    int num_read_bytes = 0;

    while(1)
    {
        // Get the number of bytes in the file descriptor,
        // copy from the file descriptor to our read buffer while not overflowing it
        // https://man7.org/linux/man-pages/man2/read.2.html
        num_read_bytes = read(
            /* int fd = */ ip_socket_file_descriptor,
            /* void buf[.count] = */ read_buffer,
            /* size_t count = */ sizeof(read_buffer));
        if(0 > num_read_bytes)
        {
            // Failed to read the file descriptor, stop this task and free its resources
            s_println("Failed to read IP packet file descriptor, stopping task to read IP packets");
            (void) close(/* int fd = */ ip_socket_file_descriptor);
            ip_socket_file_descriptor = 0;
            read_ip_packet_task_handle = nullptr;
            vTaskDelete(/* TaskHandle_t xTaskToDelete = */ NULL);
        }

        // Print the bytes from our user input to stdout (replace ending \n with \0)
        read_buffer[num_read_bytes - sizeof('\n')] = '\0';
        s_print("Got TCP packet: ");
        s_println(read_buffer);

        // See if the the packet matches a command, if so, execute it
        // All commands should be unique, no need to check against the others once a match is found
        for(size_t i = 0; i < NUM_TCP_COMMANDS; ++i)
        {
            if(((num_read_bytes - sizeof('\0')) == strlen(tcp_commands[i].command)) && 
                (0 == strncmp(read_buffer, tcp_commands[i].command, num_read_bytes)))
            {
                (*(tcp_commands[i].action))();
                break;
            }
        }

        // 29OCT2024: usStackDepth = 1024 + 512, uxTaskGetHighWaterMark = 400
        PRINT_STACK_USAGE();
    }
}

// ================================ //
// Define code to connect to TCP/IP //
// ================================ //

// Connect to an existing TCP server and read IPv4 data
// If you want to connect to IPv6 or use another protocol, please update this function
// TODO: It seems I cannot sleep while waiting to connect to TCP
bool tcp_start(
    uint32_t tcp_server_ipv4_addr,
    uint32_t tcp_server_port)
{
    // Instantiate server information
    struct sockaddr_in server_info = { 0 };
    // Give the protocol family that will be used for this server
    // AF_INET = IPv4 Internet protocols
    server_info.sin_family = AF_INET;
    // Set the IPv4 address of the TCP server to connect to
    server_info.sin_addr.s_addr = tcp_server_ipv4_addr;
    // Set the port on s_addr to connect to
    // https://linux.die.net/man/3/htons
    server_info.sin_port = htons(/* uint32_t hostlong = */ tcp_server_port);
    // Get a file descriptor to use as an endpoint for communcation
    // https://www.man7.org/linux/man-pages/man2/socket.2.html
    ip_socket_file_descriptor = socket(
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
    if (0 > ip_socket_file_descriptor)
    {
        s_println("Failed creating IP socket");
        return false;
    }

    // Connect to the server, whose IP address and port is given to us by server_info
    // https://man7.org/linux/man-pages/man2/connect.2.html
    // https://linux.die.net/man/3/inet_ntoa
    // https://man7.org/linux/man-pages/man2/close.2.html
    if (0 != connect(
        /* int sockfd = */ ip_socket_file_descriptor,
        /* const struct sockaddr *addr = */ (struct sockaddr *) &server_info,
        /* socklen_t addrlen = */ sizeof(server_info)))
    {
        s_print("Failed to connect to TCP server at: ");
        s_print(inet_ntoa(/* struct in_addr in = */ tcp_server_ipv4_addr));
        s_print(":");
        s_println(tcp_server_port, DEC);
        close(/* int fd = */ ip_socket_file_descriptor);
        ip_socket_file_descriptor = 0;
        return false;
    }

    s_print("Connected to TCP server at: ");
    s_print(inet_ntoa(/* struct in_addr in = */ tcp_server_ipv4_addr));
    s_print(":");
    s_println(tcp_server_port, DEC);

    // Create task whose job it is to read all incoming TCP packets
    (void) xTaskCreate(
        // Pointer to the task entry function. Tasks must be implemented to never return (i.e. continuous loop).
        /* TaskFunction_t pxTaskCode = */ (TaskFunction_t) task_read_ip_packets,
        // A descriptive name for the task. This is mainly used to facilitate debugging. Max length defined by configMAX_TASK_NAME_LEN - default is 16.
        /* const char *const pcName = */ "read_ip",
        // The size of the task stack specified as the NUMBER OF BYTES. Note that this differs from vanilla FreeRTOS.
        /* const configSTACK_DEPT_TYPE usStackDepth = */ 1024 + 512,
        // Pointer that will be used as the parameter for the task being created.
        /* void *const pvParameters = */ NULL,
        // The priority at which the task should run.
        // Systems that include MPU support can optionally create tasks in a privileged (system) mode by setting bit portPRIVILEGE_BIT of the priority parameter.
        // For example, to create a privileged task at priority 2 the uxPriority parameter should be set to ( 2 | portPRIVILEGE_BIT ).
        /* UBaseType_t uxPriority = */ 10,
        // Used to pass back a handle by which the created task can be referenced.
        /* TaskHandle_t *const pxCreatedTask = */ &read_ip_packet_task_handle);
    return (nullptr != read_ip_packet_task_handle);
}

// Close our connection to the connected TCP server
bool tcp_free()
{
    // Close the TCP socket, which will cause task_read_ip_packets to stop itself and clear handles
    return 0 == close(/* int fd = */ ip_socket_file_descriptor);
}

// Send a packet to the connected TCP server
bool tcp_send(
    void *packet,
    size_t num_packet_bytes,
    int flags)
{
    // Send the packet and return the result
    // See the link below for default settings, for example, this code blocks by default.
    // https://www.man7.org/linux/man-pages/man2/send.2.html
    return -1 != send(
        /* int sockfd = */ ip_socket_file_descriptor,
        /* const void buf[.len] = */ packet,
        /* size_t len = */ num_packet_bytes,
        /* int flags = */ flags);
}

#endif // WIFI_ENABLED