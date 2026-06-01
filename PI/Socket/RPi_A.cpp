/**
 * RPi_A - Proof of Concept: TCP Receiver/Dispatcher from RPi-B
 * 
 * This is a simplified TCP server that:
 * 1. Listens for TCP connections on port 5000
 * 2. Reads JSON messages line-by-line from RPi-B
 * 3. Parses JSON using cJSON
 * 4. Dispatches to device-specific handlers
 * 5. Sends ACK back via socket
 * 
 * Demonstrates: TCP server, JSON parsing, message dispatching, ACK handling
 */

#include "protocol/JsonMessage.h"
#include "dispatch/IDeviceHandler.h"
#include "dispatch/WmosHandler.h"
#include "dispatch/UnknownHandler.h"
#include "dispatch/DeviceDispatcher.h"
#include "transport/SocketConnection.h"
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace protocol;
using namespace transport;
using namespace dispatch;

/**
 * Custom handler to demonstrate dispatch (logs messages to file/console)
 */
class LoggingHandler : public IDeviceHandler {
private:
    std::string device_name_;
public:
    LoggingHandler(const std::string& device) : device_name_(device) {}
    
    void handle(const JsonMessage& message) override {
        std::cout << "[LoggingHandler] Device: " << message.getDevice()
                  << " | Sensor: " << message.getSensor()
                  << " | Data: " << message.getData() << std::endl;
    }
};

/**
 * Thread function to handle a client connection from RPi-B.
 * Reads and dispatches messages until client disconnects.
 */
void handle_rpia_client(int client_fd, DeviceDispatcher* dispatcher)
{
    char buffer[4096];
    std::cout << "[RPi_A::handle_rpia_client] Client connected, reading messages..." << std::endl;

    while (true) {
        std::memset(buffer, 0, sizeof(buffer));
        ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (n <= 0) {
            std::cout << "[RPi_A::handle_rpia_client] Client disconnected (EOF)" << std::endl;
            break;
        }

        buffer[n] = '\0';
        std::string message_line(buffer);

        // Remove trailing newline
        if (!message_line.empty() && message_line.back() == '\n') {
            message_line.pop_back();
        }

        std::cout << "[RPi_A::handle_rpia_client] Received: " << message_line << std::endl;

        // Create JsonMessage and parse
        JsonMessage msg;
        if (msg.parse(message_line)) {
            std::cout << "[RPi_A::handle_rpia_client] Parsed message - Device: " << msg.getDevice() << std::endl;
            
            // Dispatch to handlers
            dispatcher->dispatch(msg);

            // Send ACK
            std::string ack = "{\"status\":\"OK\",\"device\":\"" + msg.getDevice() + "\"}";
            send(client_fd, (ack + "\n").c_str(), ack.length() + 1, 0);
            std::cout << "[RPi_A::handle_rpia_client] Sent ACK" << std::endl;
        } else {
            std::cerr << "[RPi_A::handle_rpia_client] Failed to parse message" << std::endl;
            std::string error_ack = "{\"status\":\"ERROR\",\"reason\":\"Parse failed\"}";
            send(client_fd, (error_ack + "\n").c_str(), error_ack.length() + 1, 0);
        }
    }

    close(client_fd);
    std::cout << "[RPi_A::handle_rpia_client] Handler thread exiting" << std::endl;
}

/**
 * Thread function to listen for TCP connections on the specified port.
 */
void tcp_server_thread(int port, DeviceDispatcher* dispatcher)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "[RPi_A::tcp_server_thread] Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[RPi_A::tcp_server_thread] setsockopt failed" << std::endl;
        close(listen_fd);
        return;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[RPi_A::tcp_server_thread] Bind failed on port " << port << std::endl;
        close(listen_fd);
        return;
    }

    if (listen(listen_fd, 10) < 0) {
        std::cerr << "[RPi_A::tcp_server_thread] Listen failed" << std::endl;
        close(listen_fd);
        return;
    }

    std::cout << "[RPi_A::tcp_server_thread] TCP server listening on port " << port << std::endl;

    // Accept connections
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            std::cerr << "[RPi_A::tcp_server_thread] Accept failed" << std::endl;
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "[RPi_A::tcp_server_thread] New TCP client from " << client_ip << std::endl;

        // Handle client in thread
        std::thread client_thread(handle_rpia_client, client_fd, dispatcher);
        client_thread.detach();
    }

    close(listen_fd);
}

/**
 * Main PoC: Start TCP server and dispatch messages
 */
int main(int argc, char* argv[])
{
    std::cout << "=== RPi_A Proof of Concept ===" << std::endl;
    std::cout << "[main] TCP Receiver/Dispatcher" << std::endl;

    // Parameters
    int tcp_port = 5000;
    if (argc >= 2) {
        tcp_port = std::stoi(argv[1]);
    }

    // Create dispatcher
    std::cout << "[main] Creating device dispatcher..." << std::endl;
    auto dispatcher = std::make_unique<DeviceDispatcher>();

    // Register handlers for different device types
    auto wmos_handler = std::make_unique<WmosHandler>("wmos_handler");
    auto unknown_handler = std::make_unique<UnknownHandler>();
    auto logging_handler = std::make_unique<LoggingHandler>("generic");

    dispatcher->registerHandler("wmos_device_1", wmos_handler.get());
    dispatcher->registerHandler("wmos_device_2", wmos_handler.get());
    dispatcher->registerHandler("temperature_sensor", logging_handler.get());
    dispatcher->registerHandler("motion_detector", logging_handler.get());

    std::cout << "[main] Registered " << dispatcher->getHandlerCount() << " handlers" << std::endl;

    // Start TCP server in background thread
    std::cout << "[main] Starting TCP server on port " << tcp_port << std::endl;
    std::thread tcp_thread(tcp_server_thread, tcp_port, dispatcher.get());
    tcp_thread.detach();

    // Keep main thread alive
    std::cout << "[main] Press Ctrl+C to stop" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
