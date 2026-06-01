/**
 * RPi_B - Proof of Concept: HTTP Forwarder to RPi-A
 * 
 * This is a simplified HTTP server that:
 * 1. Listens for HTTP POST requests on port 8080
 * 2. Parses JSON body from request
 * 3. Forwards JSON message to RPi-A via TCP socket
 * 4. Reads ACK from RPi-A
 * 5. Sends HTTP response back to client
 * 
 * Demonstrates: HTTP parsing, JSON serialization, socket communication
 */

#include "dispatch/IDeviceHandler.h"
#include "dispatch/WmosHandler.h"
#include "dispatch/DeviceDispatcher.h"
#include "protocol/JsonMessage.h"
#include "transport/SocketConnection.h"
#include <iostream>
#include <memory>
#include <string>
#include <sstream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace protocol;
using namespace transport;
using namespace dispatch;

// Global persistent connection to RPi-A
std::unique_ptr<SocketConnection> g_upstream;
std::mutex g_upstream_mutex;

/**
 * Thread function to handle a single HTTP client connection.
 * Reads HTTP POST request, extracts JSON body, forwards to RPi-A.
 */
void handle_http_client(int client_fd)
{
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));

    // Read HTTP request
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        close(client_fd);
        return;
    }

    buffer[n] = '\0';
    std::string request(buffer);
    std::cout << "[RPi_B::handle_http_client] Received HTTP request (" << n << " bytes)" << std::endl;

    // Extract body from HTTP request (simple: split by \n\n)
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        header_end = request.find("\n\n");
        if (header_end == std::string::npos) {
            std::cerr << "[RPi_B::handle_http_client] Invalid HTTP format" << std::endl;
            close(client_fd);
            return;
        }
        header_end += 2;
    } else {
        header_end += 4;
    }

    std::string body = request.substr(header_end);
    std::cout << "[RPi_B::handle_http_client] Body: " << body << std::endl;

    // Forward to RPi-A
    std::string ack_response;
    {
        std::lock_guard<std::mutex> lock(g_upstream_mutex);
        if (g_upstream && g_upstream->isConnected()) {
            if (g_upstream->sendLine(body)) {
                ack_response = g_upstream->readLine();
                std::cout << "[RPi_B::handle_http_client] ACK from RPi-A: " << ack_response << std::endl;
            } else {
                std::cerr << "[RPi_B::handle_http_client] Failed to send to RPi-A" << std::endl;
                ack_response = "ERROR: Failed to forward to RPi-A";
            }
        } else {
            std::cerr << "[RPi_B::handle_http_client] Not connected to RPi-A" << std::endl;
            ack_response = "ERROR: Not connected to RPi-A";
        }
    }

    // Send HTTP response
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << ack_response.length() << "\r\n"
        << "\r\n"
        << ack_response;
    std::string response = oss.str();
    send(client_fd, response.c_str(), response.length(), 0);

    close(client_fd);
    std::cout << "[RPi_B::handle_http_client] Client disconnected" << std::endl;
}

/**
 * Thread function to listen for HTTP connections on the specified port.
 */
void http_server_thread(int port)
{
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        std::cerr << "[RPi_B::http_server_thread] Failed to create socket" << std::endl;
        return;
    }

    int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "[RPi_B::http_server_thread] setsockopt failed" << std::endl;
        close(listen_fd);
        return;
    }

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[RPi_B::http_server_thread] Bind failed on port " << port << std::endl;
        close(listen_fd);
        return;
    }

    if (listen(listen_fd, 10) < 0) {
        std::cerr << "[RPi_B::http_server_thread] Listen failed" << std::endl;
        close(listen_fd);
        return;
    }

    std::cout << "[RPi_B::http_server_thread] HTTP server listening on port " << port << std::endl;

    // Accept connections
    while (true) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            std::cerr << "[RPi_B::http_server_thread] Accept failed" << std::endl;
            continue;
        }

        std::cout << "[RPi_B::http_server_thread] New HTTP client connected" << std::endl;

        // Handle client in thread
        std::thread client_thread(handle_http_client, client_fd);
        client_thread.detach();
    }

    close(listen_fd);
}

/**
 * Main PoC: Start HTTP server and connect to RPi-A
 */
int main(int argc, char* argv[])
{
    std::cout << "=== RPi_B Proof of Concept ===" << std::endl;
    std::cout << "[main] HTTP Forwarder to RPi-A" << std::endl;

    // Parameters
    int http_port = 8080;
    std::string rpia_host = "127.0.0.1";
    int rpia_port = 5000;

    if (argc >= 2) {
        rpia_host = argv[1];
    }
    if (argc >= 3) {
        rpia_port = std::stoi(argv[2]);
    }

    // Create upstream connection to RPi-A
    std::cout << "[main] Connecting to RPi-A at " << rpia_host << ":" << rpia_port << std::endl;
    g_upstream = std::make_unique<SocketConnection>();

    if (!g_upstream->connect(rpia_host, rpia_port)) {
        std::cerr << "[main] Failed to connect to RPi-A" << std::endl;
        return 1;
    }

    std::cout << "[main] Connected to RPi-A successfully" << std::endl;

    // Start HTTP server in background thread
    std::cout << "[main] Starting HTTP server on port " << http_port << std::endl;
    std::thread http_thread(http_server_thread, http_port);
    http_thread.detach();

    // Keep main thread alive
    std::cout << "[main] Press Ctrl+C to stop" << std::endl;
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
