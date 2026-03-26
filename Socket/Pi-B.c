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
    if (write(pi2_fd, json_str, strlen(json_str)) < 0 ||
        write(pi2_fd, "\n", 1) < 0) {
        perror("ERROR writing to A");
        rc = -1;
    }
    pthread_mutex_unlock(&pi2_mutex);
    return rc;
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
        printf("[B] Received from A: %s\n", buf);
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
