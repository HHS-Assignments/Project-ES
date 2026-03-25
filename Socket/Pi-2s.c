/**
 * @file Pi-2s.c
 * @brief TCP server on Pi 2.
 *
 * Receives JSON forwarded by Pi 1, parses the Device / Button / Data fields,
 * dispatches to the appropriate device handler, and prints the result to
 * standard output.
 *
 * Adding support for a new device requires only adding a new entry to the
 * @c device_handlers table and implementing the corresponding handler function.
 *
 * Usage: ./Pi-2s <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
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

/* -------------------------------------------------------------------------
 * Device handler definitions
 * -----------------------------------------------------------------------*/

/**
 * @brief Signature for a device-specific JSON handler.
 * @param json  Parsed cJSON object for the full message (non-const because
 *              the cJSON API does not accept const pointers).
 */
typedef void (*DeviceHandlerFn)(cJSON *json);

/**
 * @brief Maps a device name string to its handler function.
 */
typedef struct {
    const char     *device_name; /**< Exact string in the "Device" field. */
    DeviceHandlerFn handler;     /**< Handler called when device matches.  */
} DeviceHandler;

/* -------------------------------------------------------------------------
 * Per-device handler implementations
 * -----------------------------------------------------------------------*/

/**
 * @brief Handler for the WMos D1 Mini device.
 *
 * Prints the Button category and the Data value received from the WMos.
 *
 * @param json  Parsed cJSON object containing Device, Button and Data.
 */
static void handle_wmos(cJSON *json)
{
    cJSON *button = cJSON_GetObjectItemCaseSensitive(json, "Button");
    cJSON *data   = cJSON_GetObjectItemCaseSensitive(json, "Data");

    printf("  [WMos] Button category : %s\n",
           (button && (button->type & cJSON_String)
                ? button->valuestring
                : "(unknown)"));

    if (data && (data->type & cJSON_String))
        printf("  [WMos] Data            : %s\n", data->valuestring);
    else if (data && (data->type & cJSON_Number))
        printf("  [WMos] Data            : %g\n", data->valuedouble);
    else
        printf("  [WMos] Data            : (unknown)\n");
}

/**
 * @brief Fallback handler for unknown / unregistered devices.
 *
 * Dumps all top-level JSON fields so nothing is silently dropped.
 *
 * @param json  Parsed cJSON object.
 */
static void handle_unknown(cJSON *json)
{
    printf("  [unknown device] Raw fields:\n");
    cJSON *item = json->child;
    while (item != NULL) {
        char *val = cJSON_PrintUnformatted(item);
        printf("    %s = %s\n", item->string ? item->string : "?",
               val ? val : "(null)");
        free(val);
        item = item->next;
    }
}

/* -------------------------------------------------------------------------
 * Device handler table
 *
 * To add a new device:
 *   1. Implement a handler function with the DeviceHandlerFn signature.
 *   2. Add a { "DeviceName", handle_new_device } entry to this table.
 * -----------------------------------------------------------------------*/
static const DeviceHandler device_handlers[] = {
    { "Wmos", handle_wmos },
    /* Add more devices here, e.g.: */
    /* { "STM32", handle_stm32 }, */
};

/** Number of registered device handlers. */
static const size_t device_handler_count =
    sizeof(device_handlers) / sizeof(device_handlers[0]);

/**
 * @brief Look up and invoke the handler for the given device name.
 *
 * Falls back to handle_unknown when no registered handler is found.
 *
 * @param device_name  Value of the "Device" JSON field.
 * @param json         Full parsed JSON object.
 */
static void dispatch(const char *device_name, cJSON *json)
{
    for (size_t i = 0; i < device_handler_count; i++) {
        if (strcmp(device_handlers[i].device_name, device_name) == 0) {
            device_handlers[i].handler(json);
            return;
        }
    }
    handle_unknown(json);
}

/* -------------------------------------------------------------------------
 * Main
 * -----------------------------------------------------------------------*/

/**
 * @brief Start the Pi 2 server, accept connections in a loop, and process
 *        JSON messages forwarded by Pi 1.
 *
 * @param argc Argument count (must be 2).
 * @param argv argv[1] must be the TCP port to listen on.
 * @return 0 on normal termination.
 */
int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno;
    socklen_t clilen;
    char buffer[4096];
    struct sockaddr_in serv_addr, cli_addr;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    portno = atoi(argv[1]);

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

    printf("Pi-2 server listening on port %d\n", portno);
    fflush(stdout);

    /* Main loop — handle one client at a time. */
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) {
            perror("ERROR on accept");
            continue;
        }

        bzero(buffer, sizeof(buffer));
        int n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n < 0) {
            perror("ERROR reading from socket");
            close(newsockfd);
            continue;
        }

        printf("--- Received message from Pi-1 (%d bytes) ---\n", n);

        cJSON *json = cJSON_Parse(buffer);
        if (json == NULL) {
            printf("  Failed to parse JSON: %s\n", buffer);
        } else {
            cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
            const char *device_name =
                (device && (device->type & cJSON_String))
                    ? device->valuestring
                    : "(unknown)";

            printf("  Device: %s\n", device_name);
            dispatch(device_name, json);
            cJSON_Delete(json);
        }

        printf("---------------------------------------------\n");
        fflush(stdout);
        close(newsockfd);
    }

    close(sockfd);
    return 0;
}
