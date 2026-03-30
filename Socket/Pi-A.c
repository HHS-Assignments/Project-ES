/**
 * @file Pi-A.c
 * @brief Full-duplex TCP server (A).
 *
 * A accepts one persistent connection from B.  Two threads run
 * concurrently over the **same** socket, making the channel fully duplex:
 *
 *  - **Reader thread** (B → A direction): reads newline-terminated JSON
 *    forwarded by B (WMos sensor events), dispatches to the appropriate
 *    device handler, and sends an acknowledgement back to B over the same
 *    socket (A → B direction).
 *
 *  - **Main thread** (A → B direction): reads lines from stdin and
 *    forwards each non-empty line unchanged to B, demonstrating that
 *    A can independently initiate communication while the reader thread
 *    is active.
 *    (In the test harness stdin is /dev/null, so the main thread exits
 *    immediately and waits for the reader to finish with pthread_join.)
 *
 * Message framing: every JSON message on the B ↔ A socket is a single
 * line terminated by @c '\\n'.
 *
 * Adding support for a new device requires only:
 *   1. Implementing a handler with the ::DeviceHandlerFn signature.
 *   2. Adding a @c { "DeviceName", handle_xxx } entry to #device_handlers.
 *
 * Usage: ./Pi-A \<port\>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "cJSON.h"
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

/* ── Shared B socket ─────────────────────────────────────────────────────── */

/** Persistent full-duplex socket to B. */
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
 * @brief Send a NUL-terminated string followed by @c '\\n' to B.
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
    /* Write the entire message reliably (handle partial writes). */
    size_t len = strlen(msg);
    size_t written = 0;
    while (written < len) {
        ssize_t w = write(pi1_fd, msg + written, len - written);
        if (w <= 0) {
            perror("ERROR writing to B");
            rc = -1;
            break;
        }
        written += (size_t)w;
    }
    if (rc == 0) {
        ssize_t w = write(pi1_fd, "\n", 1);
        if (w <= 0) {
            perror("ERROR writing newline to B");
            rc = -1;
        }
    }
    pthread_mutex_unlock(&pi1_mutex);
    return rc;
}

/* Trim leading/trailing whitespace in-place and return pointer to trimmed string. */
static char *trim(char *s)
{
    if (!s) return s;
    /* trim leading */
    while (*s && isspace((unsigned char)*s)) s++;
    /* all spaces */
    if (*s == '\0') return s;
    /* trim trailing */
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return s;
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
    /* Keep for compatibility: delegate to generic printer. */
    (void)json;
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
    /* Keep for compatibility: delegate to generic printer. */
    (void)json;
}

/*
 * Generic pretty printer: prints Device, Sensor, Data (if present), then
 * lists all top-level fields as `name = value`. This produces identical
 * formatted blocks on both Pi-A and Pi-B.
 */
static void print_generic(cJSON *json)
{
    cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
    if (device && (device->type & cJSON_String))
        printf("  Device: %s\n", device->valuestring);
    else
        printf("  Device: (unknown)\n");

    cJSON *sensor = cJSON_GetObjectItemCaseSensitive(json, "Sensor");
    if (sensor && (sensor->type & cJSON_String))
        printf("  Sensor : %s\n", sensor->valuestring);

    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "Data");
    if (data) {
        if (data->type & cJSON_Number)
            printf("  Data   : %g\n", data->valuedouble);
        else if (data->type & cJSON_String)
            printf("  Data   : %s\n", data->valuestring);
        else
            printf("  Data   : (unknown)\n");
    }

    /* Print top-level fields as key = value only when there are extra
     * fields besides Device/Sensor/Data (e.g. ACK metadata). */
    bool has_extra = false;
    cJSON *item = json->child;
    while (item) {
        if (item->string && strcmp(item->string, "Device") != 0 &&
            strcmp(item->string, "Sensor") != 0 && strcmp(item->string, "Data") != 0) {
            has_extra = true;
            break;
        }
        item = item->next;
    }
    if (has_extra) {
        item = json->child;
        while (item) {
            char *val = cJSON_PrintUnformatted(item);
            printf("    %s = %s\n",
                   item->string ? item->string : "?",
                   val ? val : "(null)");
            free(val);
            item = item->next;
        }
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
        if (strcasecmp(device_handlers[i].device_name, device_name) == 0) {
            device_handlers[i].handler(json);
            return;
        }
    }
    handle_unknown(json);
}

/* ── B reader thread ─────────────────────────────────────────────────────── */

/**
 * @brief Background thread: reads JSON lines from B and dispatches them.
 *
 * After dispatching each message the thread sends an acknowledgement back to
 * B over the same persistent socket, demonstrating the A → B
 * direction of the full-duplex channel.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void *pi1_reader_thread(void *arg)
{
    char buf[4096];
    (void)arg;

    printf("[A] B reader thread started\n");
    fflush(stdout);

    while (1) {
        int n = read_line(pi1_fd, buf, sizeof(buf));
        if (n <= 0) {
            printf("[A] B disconnected\n");
            fflush(stdout);
            break;
        }

        printf("--- Received from B (%d bytes) ---\n", n);

        cJSON *json = cJSON_Parse(buf);
        if (!json) {
            printf("  Failed to parse JSON: %s\n", buf);
        } else {
            cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
            const char *device_name =
                (device && (device->type & cJSON_String))
                    ? device->valuestring
                    : "(unknown)";

                        /* Use generic printer for uniform output (avoid duplicate Device lines) */
                        print_generic(json);

                        /* Send acknowledgement back to B (A → B direction), but do not
                         * reply to incoming ACK messages themselves to avoid an
                         * acknowledgement loop. */
                        cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");
                        if (!(status && (status->type & cJSON_String) &&
                                    strcmp(status->valuestring, "ack") == 0)) {
                                pi1_send("{\"status\":\"ack\",\"Device\":\"A\"}");
                        }

                        cJSON_Delete(json);
        }

        printf("-------------------------------------\n");
        fflush(stdout);
    }
    return NULL;
}

/* ── main ────────────────────────────────────────────────────────────────── */

/**
 * @brief Entry point for A.
 *
 * 1. Binds @p port and accepts one persistent connection from B.
 * 2. Spawns the B reader thread.
 * 3. Reads non-empty lines from stdin and forwards them unchanged to B
 *    to demonstrate the
 *    A → B direction independently of the reader thread.
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
    printf("[A] Listening on port %d - waiting for B...\n", portno);
    fflush(stdout);

    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    pi1_fd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
    if (pi1_fd < 0)
        error("ERROR on accept");

    printf("[A] B connected (full-duplex)\n");
    fflush(stdout);

    /* Listening socket no longer needed - only one B connection. */
    close(sockfd);

    /* ── Start reader thread (B -> A direction + ACK back) ── */
    pthread_t reader;
    if (pthread_create(&reader, NULL, pi1_reader_thread, NULL) != 0)
        error("ERROR creating B reader thread");

    /* ── Main thread: forward stdin lines to B (A -> B direction) ── */
    /* ── Main thread: forward stdin lines to B (A -> B direction) ── */
    /* Lines typed as CSV: Device,Sensor,Data -> converted to compact JSON */
    char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        /* Strip trailing newline. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n')
            line[len - 1] = '\0';
        if (strlen(line) == 0)
            continue;

        /* Convert comma-separated commands into JSON when appropriate. */
        char *p = strchr(line, ',');
        if (p) {
            /* Parse up to three CSV fields: Device,Sensor,Data */
            char *dev = strtok(line, ",");
            char *sen = strtok(NULL, ",");
            char *dat = strtok(NULL, ",");
            if (dev && sen && dat) {
                /* Trim whitespace around CSV fields */
                dev = trim(dev);
                sen = trim(sen);
                dat = trim(dat);

                cJSON *obj = cJSON_CreateObject();
                cJSON_AddStringToObject(obj, "Device", dev);
                cJSON_AddStringToObject(obj, "Sensor", sen);
                /* Try numeric Data, fall back to string */
                char *endptr = NULL;
                double v = strtod(dat, &endptr);
                if (endptr && *endptr == '\0')
                    cJSON_AddNumberToObject(obj, "Data", v);
                else
                    cJSON_AddStringToObject(obj, "Data", dat);

                char *json_str = cJSON_PrintUnformatted(obj);
                cJSON_Delete(obj);
                if (json_str) {
                    pi1_send(json_str);
                    printf("[A] Sent JSON to B: %s\n", json_str);
                    fflush(stdout);
                    free(json_str);
                    continue;
                }
            }
        }

        /* Fallback: forward raw line unchanged (assume already JSON). */
        pi1_send(line);
        printf("[A] Sent to B: %s\n", line);
        fflush(stdout);
    }

    /* Wait for B to disconnect (or process to be killed in tests). */
    pthread_join(reader, NULL);

    close(pi1_fd);
    return 0;
}
