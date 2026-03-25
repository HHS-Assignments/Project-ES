/**
 * @file Pi-1s.c
 * @brief TCP server on Pi 1.
 *
 * Listens on the given port for HTTP POST requests sent by the WMos D1 Mini.
 * Each request carries a JSON body with the fields Device, Button and Data.
 * The JSON is parsed, printed, and forwarded to Pi 2 over a TCP socket.
 *
 * Usage: ./Pi-1s <port> <pi2_host> <pi2_port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "cJSON.h"

/**
 * @brief Print an error message and terminate the process.
 * @param msg Error text shown with perror.
 */
void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/**
 * @brief Extract the HTTP body from a raw HTTP request.
 *
 * HTTP headers are separated from the body by a blank line (CRLFCRLF or LFLF).
 *
 * @param request  Raw HTTP request buffer.
 * @return Pointer into @p request at the start of the body,
 *         or @p request itself when no HTTP header separator is found
 *         (raw JSON fallback).
 */
static const char *extract_http_body(const char *request)
{
    const char *body = strstr(request, "\r\n\r\n");
    if (body != NULL) {
        return body + 4;
    }
    body = strstr(request, "\n\n");
    if (body != NULL) {
        return body + 2;
    }
    /* No HTTP headers found – treat the whole buffer as JSON. */
    return request;
}

/**
 * @brief Send an HTTP 200 OK response with a JSON body.
 * @param sockfd        Client socket descriptor.
 * @param json_response JSON string to include as the response body.
 */
static void send_http_response(int sockfd, const char *json_response)
{
    char header[256];
    int  body_len = (int)strlen(json_response);
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             body_len);
    if (write(sockfd, header, strlen(header)) < 0)
        perror("WARNING write header");
    if (write(sockfd, json_response, body_len) < 0)
        perror("WARNING write body");
}

/**
 * @brief Connect to Pi 2 and forward a JSON string.
 * @param pi2_host Hostname or IP address of Pi 2.
 * @param pi2_port TCP port that Pi 2 is listening on.
 * @param json_str Compact JSON string to send.
 * @return 0 on success, -1 on any network error.
 */
static int forward_to_pi2(const char *pi2_host, int pi2_port,
                           const char *json_str)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket to Pi2");
        return -1;
    }

    server = gethostbyname(pi2_host);
    if (server == NULL) {
        fprintf(stderr, "ERROR: no such host %s\n", pi2_host);
        close(sockfd);
        return -1;
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&serv_addr.sin_addr.s_addr,
          server->h_length);
    serv_addr.sin_port = htons(pi2_port);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting to Pi2");
        close(sockfd);
        return -1;
    }

    int n = write(sockfd, json_str, strlen(json_str));
    if (n < 0) {
        perror("ERROR writing to Pi2 socket");
        close(sockfd);
        return -1;
    }

    printf("Forwarded to Pi2: %s\n", json_str);
    close(sockfd);
    return 0;
}

/**
 * @brief Main entry point for the Pi 1 server.
 *
 * Accepts connections in a loop, extracts the JSON body from each HTTP POST,
 * parses the Device / Button / Data fields, and forwards the JSON to Pi 2.
 *
 * @param argc Argument count (must be 4).
 * @param argv argv[1] = listen port, argv[2] = Pi2 host, argv[3] = Pi2 port.
 * @return 0 on normal termination.
 */
int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    /* 4096 bytes: enough for a typical HTTP POST header (~500 B) plus a
     * JSON payload of up to ~3500 B – well above any WMos message. */
    char buffer[4096];
    struct sockaddr_in serv_addr, cli_addr;
    int n;

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <port> <pi2_host> <pi2_port>\n", argv[0]);
        exit(1);
    }

    portno        = atoi(argv[1]);
    const char *pi2_host = argv[2];
    int         pi2_port = atoi(argv[3]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    /* Allow the port to be reused immediately after restart. */
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    printf("Pi-1 server listening on port %d, forwarding to %s:%d\n",
           portno, pi2_host, pi2_port);

    /* Main loop — handle one client at a time. */
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            continue;
        }

        bzero(buffer, sizeof(buffer));
        n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n < 0) {
            perror("ERROR reading from socket");
            close(newsockfd);
            continue;
        }

        printf("Received request (%d bytes)\n", n);

        /* Skip HTTP headers; fall back to raw JSON when no headers present. */
        const char *json_body = extract_http_body(buffer);

        printf("JSON body: %s\n", json_body);

        /* Parse and validate JSON. */
        cJSON *json = cJSON_Parse(json_body);
        if (json == NULL) {
            printf("Failed to parse JSON\n");
            send_http_response(newsockfd, "{\"error\":\"Invalid JSON\"}");
        } else {
            cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
            cJSON *button = cJSON_GetObjectItemCaseSensitive(json, "Button");
            cJSON *data   = cJSON_GetObjectItemCaseSensitive(json, "Data");

            if (device && (device->type & cJSON_String))
                printf("Device: %s\n", device->valuestring);
            if (button && (button->type & cJSON_String))
                printf("Button: %s\n", button->valuestring);
            if (data && (data->type & cJSON_String))
                printf("Data: %s\n", data->valuestring);
            else if (data && (data->type & cJSON_Number))
                printf("Data: %g\n", data->valuedouble);

            /* Forward compact JSON to Pi 2. */
            char *forward_json = cJSON_PrintUnformatted(json);
            if (forward_json) {
                int result = forward_to_pi2(pi2_host, pi2_port, forward_json);
                if (result == 0)
                    send_http_response(newsockfd,
                                       "{\"status\":\"ok\",\"forwarded\":true}");
                else
                    send_http_response(newsockfd,
                                       "{\"status\":\"error\",\"forwarded\":false}");
                free(forward_json);
            }
            cJSON_Delete(json);
        }

        close(newsockfd);
    }

    close(sockfd);
    return 0;
}

