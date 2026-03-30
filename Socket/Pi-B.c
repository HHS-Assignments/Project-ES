/**
 * @file Pi-B.c
 * @brief Full-duplex TCP client (B).
 *
 * B fulfils two roles simultaneously using POSIX threads:
 *
 *  - **HTTP server** (WMos-facing, multi-threaded): listens for JSON POST
 *    requests sent by the WMos D1 Mini.  Each accepted connection is handed
 *    off to a dedicated worker thread, so multiple WMos devices (or rapid
 *    successive requests from one device) are handled concurrently.  Each
 *    worker parses Device / Sensor / Data and forwards the compact JSON to
 *    A over the persistent socket.
 *
 *  - **Full-duplex TCP client** (A-facing): maintains one persistent
 *    connection to A for the lifetime of the process.  A dedicated reader
 *    thread receives messages sent back by A (acknowledgements, commands)
 *    concurrently with outgoing forwards, making the channel truly
 *    bidirectional.
 *
 * Message framing on the B ↔ A socket: every JSON message is sent as
 * a single line terminated by @c '\\n'.  The reader uses a byte-at-a-time
 * @c read_line() helper to reconstruct complete messages.
 *
 * Usage: ./Pi-B \<wmos_port\> \<a_host\> \<a_port\>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "cJSON.h"
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>

/** Buffer size for incoming HTTP POST requests from the WMos. */
#define HTTP_BUFFER_SIZE 4096

/* ── Shared A socket ─────────────────────────────────────────────────────── */

/** Persistent full-duplex socket to A. */
static int             pi2_fd    = -1;
/** Protects all writes to @c pi2_fd. */
static pthread_mutex_t pi2_mutex = PTHREAD_MUTEX_INITIALIZER;

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
 * @brief Send a NUL-terminated string followed by @c '\\n' to A.
 *
 * Thread-safe: all writes are serialised by @c pi2_mutex.
 *
 * @param json_str Compact JSON string to send.
 * @return 0 on success, -1 on write error.
 */
static int pi2_send(const char *json_str)
{
    int rc = 0;
    pthread_mutex_lock(&pi2_mutex);
    /* Ensure full write of the JSON string and terminating newline. */
    size_t len = strlen(json_str);
    size_t written = 0;
    while (written < len) {
        ssize_t w = write(pi2_fd, json_str + written, len - written);
        if (w <= 0) {
            perror("ERROR writing to A");
            rc = -1;
            break;
        }
        written += (size_t)w;
    }
    if (rc == 0) {
        ssize_t w = write(pi2_fd, "\n", 1);
        if (w <= 0) {
            perror("ERROR writing newline to A");
            rc = -1;
        }
    }
    pthread_mutex_unlock(&pi2_mutex);
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

/* ── Device handler definitions (mirror Pi-A for consistent console output) */

typedef void (*DeviceHandlerFn)(cJSON *json);

typedef struct {
    const char *device_name;
    DeviceHandlerFn handler;
} DeviceHandler;

static void handle_wmos(cJSON *json)
{
    (void)json; /* delegated to generic printer */
}

static void handle_unknown(cJSON *json)
{
    (void)json; /* delegated to generic printer */
}

/* Generic printer to produce consistent output on both sides. */
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

static const DeviceHandler device_handlers[] = {
    { "Wmos", handle_wmos },
};

static const size_t device_handler_count =
    sizeof(device_handlers) / sizeof(device_handlers[0]);

static void dispatch(const char *device_name, cJSON *json)
{
    /* Ignore device-specific handlers; use generic printer for uniform output. */
    (void)device_name;
    print_generic(json);
}

/* ── HTTP helpers ────────────────────────────────────────────────────────── */

/**
 * @brief Return a pointer to the HTTP body inside @p request.
 *
 * Skips past the blank line (CRLFCRLF or LFLF) that separates headers from
 * the body.  If no separator is found the entire buffer is treated as raw
 * JSON.
 *
 * @param request Raw HTTP request buffer.
 * @return Pointer into @p request at the start of the body.
 */
static const char *extract_http_body(const char *request)
{
    const char *body = strstr(request, "\r\n\r\n");
    if (body)
        return body + 4;
    body = strstr(request, "\n\n");
    if (body)
        return body + 2;
    return request;
}

/**
 * @brief Send an HTTP 200 OK response with a JSON body to @p sockfd.
 * @param sockfd        Client socket descriptor.
 * @param json_response JSON string used as the response body.
 */
static void send_http_response(int sockfd, const char *json_response)
{
    char header[256];
    int  len = (int)strlen(json_response);
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n",
             len);
    if (write(sockfd, header, strlen(header)) < 0)
        perror("WARNING write header");
    if (write(sockfd, json_response, len) < 0)
        perror("WARNING write body");
}

/* ── A reader thread ─────────────────────────────────────────────────────── */

/**
 * @brief Background thread: continuously reads JSON lines from A.
 *
 * Receives acknowledgements and commands sent by A over the persistent
 * full-duplex socket and prints them to standard output.  This thread runs
 * concurrently with the WMos HTTP accept loop in main(), enabling true
 * bidirectional communication.
 *
 * @param arg Unused.
 * @return NULL.
 */
static void *pi2_reader_thread(void *arg)
{
    char buf[4096];
    (void)arg;

    printf("[B] A reader thread started\n");
    fflush(stdout);

    while (1) {
        int n = read_line(pi2_fd, buf, sizeof(buf));
        if (n <= 0) {
            printf("[B] A connection closed\n");
            fflush(stdout);
            break;
        }
        printf("--- Received from A (%d bytes) ---\n", n);

        cJSON *json = cJSON_Parse(buf);
        if (!json) {
            printf("  Failed to parse JSON: %s\n", buf);
        } else {
                cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");

                /* If this is an ACK, print only the concise ACK line.
                 * For non-ACK messages, print the full generic block. */
                if (status && (status->type & cJSON_String) &&
                    strcmp(status->valuestring, "ack") == 0) {
                    printf("[B] Received ACK from A\n");
                } else {
                    /* Use generic printer for uniform output (avoid duplicate Device lines) */
                    print_generic(json);
                    printf("[B] Received Data from A\n");
                    /* Send ACK back to A identifying this side as B. */
                    pi2_send("{\"status\":\"ack\",\"Device\":\"B\"}");
                }

            cJSON_Delete(json);
        }

        printf("-------------------------------------\n");
        fflush(stdout);
    }
    return NULL;
}

/* ── Stdin sender thread (B -> A direction) ───────────────────────────────── */

/**
 * @brief Background thread: reads lines from stdin and sends them to A.
 *
 * Lines may be typed as CSV: Device,Sensor,Data and will be converted to
 * compact JSON before sending. Otherwise raw lines are forwarded unchanged.
 */
static void *pi2_stdin_thread(void *arg)
{
    char line[1024];
    (void)arg;

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
                char *endptr = NULL;
                double v = strtod(dat, &endptr);
                if (endptr && *endptr == '\0')
                    cJSON_AddNumberToObject(obj, "Data", v);
                else
                    cJSON_AddStringToObject(obj, "Data", dat);

                char *json_str = cJSON_PrintUnformatted(obj);
                cJSON_Delete(obj);
                if (json_str) {
                    int rc = pi2_send(json_str);
                    printf("[B] Sent JSON to A: %s\n", json_str);
                    fflush(stdout);
                    free(json_str);
                    if (rc != 0)
                        fprintf(stderr, "[B] Warning: failed to send to A\n");
                    continue;
                }
            }
        }

        /* Fallback: forward raw line unchanged (assume already JSON). */
        if (pi2_send(line) == 0)
            printf("[B] Sent to A: %s\n", line);
        else
            fprintf(stderr, "[B] Warning: failed to send to A\n");
        fflush(stdout);
    }

    return NULL;
}

/* ── Per-WMos-connection thread ──────────────────────────────────────────── */

/**
 * @brief Arguments passed to each WMos client worker thread.
 */
typedef struct {
    int client_fd; /**< Accepted client socket for one WMos HTTP connection. */
} WmosClientArgs;

/**
 * @brief Worker thread: handles one WMos HTTP POST connection.
 *
 * Reads the HTTP request, extracts the JSON body, parses Device / Sensor /
 * Data, and forwards the compact JSON to A via the persistent socket.
 * Sends an appropriate HTTP response and closes the client socket before
 * returning.
 *
 * The thread is always detached; the caller must not join it.
 *
 * @param arg  Pointer to a heap-allocated WmosClientArgs; freed here.
 * @return NULL.
 */
static void *wmos_client_thread(void *arg)
{
    WmosClientArgs *a = (WmosClientArgs *)arg;
    int client = a->client_fd;
    free(a);

    char buffer[HTTP_BUFFER_SIZE];
    memset(buffer, 0, sizeof(buffer));
    int n = read(client, buffer, HTTP_BUFFER_SIZE - 1);
    if (n < 0) {
        perror("ERROR reading from WMos");
        close(client);
        return NULL;
    }

    const char *json_body = extract_http_body(buffer);
    printf("[B] WMos request: %s\n", json_body);
    fflush(stdout);

    cJSON *json = cJSON_Parse(json_body);
    if (!json) {
        printf("[B] Invalid JSON from WMos\n");
        fflush(stdout);
        send_http_response(client, "{\"error\":\"Invalid JSON\"}");
    } else {
        cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
        cJSON *sensor = cJSON_GetObjectItemCaseSensitive(json, "Sensor");
        cJSON *data   = cJSON_GetObjectItemCaseSensitive(json, "Data");

        if (device && (device->type & cJSON_String))
            printf("[B] Device: %s\n", device->valuestring);
        if (sensor && (sensor->type & cJSON_String))
            printf("[B] Sensor: %s\n", sensor->valuestring);
        if (data && (data->type & cJSON_String))
            printf("[B] Data: %s\n", data->valuestring);
        else if (data && (data->type & cJSON_Number))
            printf("[B] Data: %g\n", data->valuedouble);

        char *compact = cJSON_PrintUnformatted(json);
        if (compact) {
            int rc = pi2_send(compact);
            if (rc == 0)
                send_http_response(client,
                                   "{\"status\":\"ok\",\"forwarded\":true}");
            else
                send_http_response(client,
                                   "{\"status\":\"error\",\"forwarded\":false}");
            free(compact);
        }
        cJSON_Delete(json);
    }

    close(client);
    return NULL;
}

/* ── main ────────────────────────────────────────────────────────────────── */

/**
 * @brief Entry point for B.
 *
 * 1. Connects to A and spawns the A reader thread.
 * 2. Binds the WMos HTTP listening socket.
 * 3. Accepts WMos connections in a loop; each connection is handed to a
 *    dedicated detached worker thread (wmos_client_thread()) so that
 *    concurrent WMos requests are processed in parallel.
 *
 * @param argc Argument count (must be 4).
 * @param argv argv[1] = WMos listen port, argv[2] = A host,
 *             argv[3] = A port.
 * @return 0 on normal termination.
 */
int main(int argc, char *argv[])
{
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <wmos_port> <pi2_host> <pi2_port>\n",
                argv[0]);
        exit(1);
    }

    int         wmos_port = atoi(argv[1]);
    const char *pi2_host  = argv[2];
    int         pi2_port  = atoi(argv[3]);

    /* ── Connect to A (persistent full-duplex) ── */
    struct hostent *server = gethostbyname(pi2_host);
    if (!server) {
        fprintf(stderr, "ERROR: no such host '%s'\n", pi2_host);
        exit(1);
    }

    pi2_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (pi2_fd < 0)
        error("ERROR opening A socket");

    struct sockaddr_in pi2_addr;
    bzero(&pi2_addr, sizeof(pi2_addr));
    pi2_addr.sin_family = AF_INET;
    bcopy(server->h_addr, &pi2_addr.sin_addr.s_addr, server->h_length);
    pi2_addr.sin_port = htons(pi2_port);

    if (connect(pi2_fd, (struct sockaddr *)&pi2_addr, sizeof(pi2_addr)) < 0)
        error("ERROR connecting to A");

    printf("[B] Connected to A at %s:%d (full-duplex)\n",
           pi2_host, pi2_port);
    fflush(stdout);

    /* ── Start background thread to receive A messages ── */
    pthread_t reader;
    if (pthread_create(&reader, NULL, pi2_reader_thread, NULL) != 0)
        error("ERROR creating A reader thread");
    pthread_detach(reader);

    /* Start background thread to read stdin and forward commands to A */
    pthread_t stdin_thr;
    if (pthread_create(&stdin_thr, NULL, pi2_stdin_thread, NULL) != 0)
        error("ERROR creating stdin sender thread");
    pthread_detach(stdin_thr);

    /* ── Bind WMos HTTP listening socket ── */
    int wmos_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (wmos_fd < 0)
        error("ERROR opening WMos socket");

    int opt = 1;
    setsockopt(wmos_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in wmos_addr;
    bzero(&wmos_addr, sizeof(wmos_addr));
    wmos_addr.sin_family      = AF_INET;
    wmos_addr.sin_addr.s_addr = INADDR_ANY;
    wmos_addr.sin_port        = htons(wmos_port);

    if (bind(wmos_fd, (struct sockaddr *)&wmos_addr, sizeof(wmos_addr)) < 0)
        error("ERROR on bind");

    listen(wmos_fd, 5);
    printf("[B] Listening for WMos on port %d (multi-threaded)\n",
           wmos_port);
    fflush(stdout);

    /* ── Main loop: accept WMos connections, one thread per connection ── */
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int client = accept(wmos_fd, (struct sockaddr *)&cli_addr, &clilen);
        if (client < 0) {
            perror("ERROR on accept");
            continue;
        }

        WmosClientArgs *args = malloc(sizeof(WmosClientArgs));
        if (!args) {
            perror("ERROR allocating WMos thread args");
            close(client);
            continue;
        }
        args->client_fd = client;

        pthread_t wt;
        if (pthread_create(&wt, NULL, wmos_client_thread, args) != 0) {
            perror("ERROR creating WMos client thread");
            close(client);
            free(args);
            continue;
        }
        pthread_detach(wt);
    }

    close(wmos_fd);
    close(pi2_fd);
    return 0;
}
