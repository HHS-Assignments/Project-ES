/**
 * @file Pi-2.c
 * @brief Full-duplex TCP server (Pi 2).
 *
 * Pi-2 accepts one persistent connection from Pi-1.  Two threads run
 * concurrently over the **same** socket, making the channel fully duplex:
 *
 *  - **Reader thread** (Pi-1 → Pi-2 direction): reads newline-terminated JSON
 *    forwarded by Pi-1 (WMos sensor events), dispatches to the appropriate
 *    device handler, and sends an acknowledgement back to Pi-1 over the same
 *    socket (Pi-2 → Pi-1 direction).
 *
 *  - **Main thread** (Pi-2 → Pi-1 direction): reads lines from stdin and
 *    forwards them as JSON to Pi-1, demonstrating that Pi-2 can independently
 *    initiate communication while the reader thread is active.
 *    (In the test harness stdin is /dev/null, so the main thread exits
 *    immediately and waits for the reader to finish with pthread_join.)
 *
 * Message framing: every JSON message on the Pi-1 ↔ Pi-2 socket is a single
 * line terminated by @c '\\n'.
 *
 * Adding support for a new device requires only:
 *   1. Implementing a handler with the ::DeviceHandlerFn signature.
 *   2. Adding a @c { "DeviceName", handle_xxx } entry to #device_handlers.
 *
 * Usage: ./Pi-2 \<port\>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "cJSON.h"

/* ── Shared Pi-1 socket ──────────────────────────────────────────────────── */

/** Persistent full-duplex socket to Pi-1. */
static int             pi1_fd    = -1;
/** Protects all writes to @c pi1_fd. */
static pthread_mutex_t pi1_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Generic helpers ─────────────────────────────────────────────────────── */

/**
 * @brief Print an error message and terminate the process.
 * @param msg Error text forwarded to perror.
 */
static void error(const char *msg)
{
    perror(msg);
    exit(1);
}

/**
 * @brief Read one @c '\\n'-terminated line from @p fd into @p buf.
 *
 * The newline is consumed but not stored; @p buf is always NUL-terminated.
 *
 * @param fd   Socket file descriptor.
 * @param buf  Destination buffer.
 * @param size Capacity of @p buf in bytes (including the NUL terminator).
 * @return Number of characters stored (>0), 0 on EOF, -1 on I/O error.
 */
static int read_line(int fd, char *buf, int size)
{
    int  i = 0;
    char c;
    while (i < size - 1) {
        int n = read(fd, &c, 1);
        if (n <= 0)
            return n; /* 0 = EOF, -1 = error */
        if (c == '\n')
            break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

/**
 * @brief Send a NUL-terminated string followed by @c '\\n' to Pi-1.
 *
 * Thread-safe: all writes are serialised by @c pi1_mutex.
 *
 * @param msg Message to send (should be valid compact JSON).
 * @return 0 on success, -1 on write error.
 */
static int pi1_send(const char *msg)
{
    int rc = 0;
    pthread_mutex_lock(&pi1_mutex);
    if (write(pi1_fd, msg, strlen(msg)) < 0 ||
        write(pi1_fd, "\n", 1) < 0) {
        perror("ERROR writing to Pi-1");
        rc = -1;
    }
    pthread_mutex_unlock(&pi1_mutex);
    return rc;
}

/* ── Device handler definitions ──────────────────────────────────────────── */

/**
 * @brief Signature for a device-specific JSON message handler.
 * @param json  Parsed cJSON object for the full message.
 */
typedef void (*DeviceHandlerFn)(cJSON *json);

/**
 * @brief Maps a device name string to its handler function.
 */
typedef struct {
    const char     *device_name; /**< Exact string in the "Device" field. */
    DeviceHandlerFn handler;     /**< Handler called when device matches.  */
} DeviceHandler;

/* ── Per-device handler implementations ─────────────────────────────────── */

/**
 * @brief Handler for the WMos D1 Mini device.
 *
 * Prints the Sensor name and Data value received from the WMos.
 *
 * @param json  Parsed cJSON object containing Device, Sensor and Data.
 */
static void handle_wmos(cJSON *json)
{
    cJSON *sensor = cJSON_GetObjectItemCaseSensitive(json, "Sensor");
    cJSON *data   = cJSON_GetObjectItemCaseSensitive(json, "Data");

    printf("  [WMos] Sensor : %s\n",
           (sensor && (sensor->type & cJSON_String))
               ? sensor->valuestring
               : "(unknown)");

    if (data && (data->type & cJSON_String))
        printf("  [WMos] Data   : %s\n", data->valuestring);
    else if (data && (data->type & cJSON_Number))
        printf("  [WMos] Data   : %g\n", data->valuedouble);
    else
        printf("  [WMos] Data   : (unknown)\n");
}

/**
 * @brief Fallback handler for unknown / unregistered devices.
 *
 * Dumps all top-level JSON fields so nothing is silently discarded.
 *
 * @param json  Parsed cJSON object.
 */
static void handle_unknown(cJSON *json)
{
    printf("  [unknown device] Raw fields:\n");
    cJSON *item = json->child;
    while (item) {
        char *val = cJSON_PrintUnformatted(item);
        printf("    %s = %s\n",
               item->string ? item->string : "?",
               val ? val : "(null)");
        free(val);
        item = item->next;
    }
}

/* ── Device handler table ────────────────────────────────────────────────── */

/**
 * @brief Registered device handlers.
 *
 * To add a new device:
 *   1. Implement a handler function with the ::DeviceHandlerFn signature.
 *   2. Add a @c { "DeviceName", handle_xxx } entry to this table.
 */
static const DeviceHandler device_handlers[] = {
    { "Wmos", handle_wmos },
    /* { "STM32", handle_stm32 }, */
};

/** Number of registered device handlers. */
static const size_t device_handler_count =
    sizeof(device_handlers) / sizeof(device_handlers[0]);

/**
 * @brief Look up and invoke the handler for @p device_name.
 *
 * Falls back to handle_unknown() when no registered handler is found.
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

/* ── Pi-1 reader thread ──────────────────────────────────────────────────── */

/**
 * @brief Background thread: reads JSON lines from Pi-1 and dispatches them.
 *
 * After dispatching each message the thread sends an acknowledgement back to
 * Pi-1 over the same persistent socket, demonstrating the Pi-2 → Pi-1
 * direction of the full-duplex channel.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void *pi1_reader_thread(void *arg)
{
    char buf[4096];
    (void)arg;

    printf("[Pi-2] Pi-1 reader thread started\n");
    fflush(stdout);

    while (1) {
        int n = read_line(pi1_fd, buf, sizeof(buf));
        if (n <= 0) {
            printf("[Pi-2] Pi-1 disconnected\n");
            fflush(stdout);
            break;
        }

        printf("--- Received from Pi-1 (%d bytes) ---\n", n);

        cJSON *json = cJSON_Parse(buf);
        if (!json) {
            printf("  Failed to parse JSON: %s\n", buf);
        } else {
            cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
            const char *device_name =
                (device && (device->type & cJSON_String))
                    ? device->valuestring
                    : "(unknown)";

            printf("  Device: %s\n", device_name);
            dispatch(device_name, json);
            cJSON_Delete(json);

            /* Send acknowledgement back to Pi-1 (Pi-2 → Pi-1 direction). */
            pi1_send("{\"status\":\"ack\",\"Device\":\"Pi2\"}");
        }

        printf("-------------------------------------\n");
        fflush(stdout);
    }
    return NULL;
}

/* ── main ────────────────────────────────────────────────────────────────── */

/**
 * @brief Entry point for Pi-2.
 *
 * 1. Binds @p port and accepts one persistent connection from Pi-1.
 * 2. Spawns the Pi-1 reader thread.
 * 3. Reads lines from stdin and forwards them to Pi-1 to demonstrate the
 *    Pi-2 → Pi-1 direction independently of the reader thread.
 *    When stdin reaches EOF (or is /dev/null in tests), the main thread
 *    waits for the reader thread to finish before exiting.
 *
 * @param argc Argument count (must be 2).
 * @param argv argv[1] must be the TCP port to listen on.
 * @return 0 on normal termination.
 */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int portno = atoi(argv[1]);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        error("ERROR opening socket");

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(portno);

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    listen(sockfd, 1);
    printf("[Pi-2] Listening on port %d — waiting for Pi-1...\n", portno);
    fflush(stdout);

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    pi1_fd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (pi1_fd < 0)
        error("ERROR on accept");

    printf("[Pi-2] Pi-1 connected (full-duplex)\n");
    fflush(stdout);

    /* Listening socket no longer needed — only one Pi-1 connection. */
    close(sockfd);

    /* ── Start reader thread (Pi-1 → Pi-2 direction + ACK back) ── */
    pthread_t reader;
    if (pthread_create(&reader, NULL, pi1_reader_thread, NULL) != 0)
        error("ERROR creating Pi-1 reader thread");

    /* ── Main thread: forward stdin lines to Pi-1 (Pi-2 → Pi-1 direction) ── */
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        /* Strip trailing newline. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (strlen(line) == 0)
            continue;
        pi1_send(line);
        printf("[Pi-2] Sent to Pi-1: %s\n", line);
        fflush(stdout);
    }

    /* Wait for Pi-1 to disconnect (or process to be killed in tests). */
    pthread_join(reader, NULL);

    close(pi1_fd);
    return 0;
}
