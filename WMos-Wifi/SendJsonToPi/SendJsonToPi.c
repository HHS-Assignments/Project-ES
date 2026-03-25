/**
 * @file SendJsonToPi.c
 * @brief Pure-C implementation of the SendJsonToPi library.
 *
 * Builds a JSON payload and sends it to Pi-1 as a raw HTTP/1.1 POST request
 * over a TCP socket using the lwIP BSD socket API that the ESP8266 Arduino
 * platform exposes to C translation units.
 *
 * No C++ or Arduino-framework types are used here; the file compiles cleanly
 * as strict C99/C11.
 *
 * @note When compiled outside the Arduino environment (e.g. for unit tests on
 *       a Linux host), define @c POSIX_SOCKETS to use the standard POSIX
 *       headers instead of the lwIP ones:
 * @code
 *   gcc -DPOSIX_SOCKETS SendJsonToPi.c -o test
 * @endcode
 */

#include "SendJsonToPi.h"

#include <stdio.h>
#include <string.h>

#ifdef POSIX_SOCKETS
/* Linux / macOS host build (for unit tests). */
#  include <sys/socket.h>
#  include <netdb.h>
#  include <unistd.h>
#else
/* ESP8266 Arduino target build. */
#  include <lwip/sockets.h>
#  include <lwip/netdb.h>
#endif

/* ── Buffer sizes ───────────────────────────────────────────────────────── */

/** Maximum size of the JSON payload string (device + sensor + data). */
#define JSON_BUF_SIZE  256

/**
 * Maximum size of the full HTTP request (headers + JSON body).
 * Headers are ~140 bytes; JSON is at most JSON_BUF_SIZE bytes.
 */
#define HTTP_BUF_SIZE  512

/** Maximum size of the HTTP response buffer (status line + body). */
#define RESP_BUF_SIZE  256

/* ── Default connection configuration ──────────────────────────────────── */

/** Default Pi-1 hostname/IP. Reassign in setup() to change. */
const char *piHost = "10.0.42.1";

/** Default Pi-1 TCP port. Reassign in setup() to change. */
int piPort = 9000;

/* ── Internal helper ────────────────────────────────────────────────────── */

/**
 * @brief Perform the actual HTTP POST with the given JSON payload.
 *
 * Opens a TCP connection to @c piHost:piPort, sends a minimal HTTP/1.1 POST
 * request with the supplied JSON body, reads the response, and closes the
 * socket.  All error conditions are printed and handled gracefully; the
 * function never terminates the process.
 *
 * @param json_payload  NUL-terminated JSON string to POST.
 */
static void post_payload(const char *json_payload)
{
    struct hostent     *he;
    struct sockaddr_in  addr;
    int                 fd;
    int                 json_len;
    char                http_request[HTTP_BUF_SIZE];
    char                response[RESP_BUF_SIZE];
    int                 n;

    json_len = (int)strlen(json_payload);

    /* Build the HTTP request. */
    n = snprintf(http_request, sizeof(http_request),
                 "POST / HTTP/1.1\r\n"
                 "Host: %s:%d\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %d\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s",
                 piHost, piPort, json_len, json_payload);

    if (n < 0 || n >= (int)sizeof(http_request)) {
        printf("[SendJsonToPi] Payload too large, request truncated.\n");
        return;
    }

    /* Resolve hostname. */
    he = gethostbyname(piHost);
    if (!he) {
        printf("[SendJsonToPi] DNS lookup failed for %s\n", piHost);
        return;
    }

    /* Open TCP socket. */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("[SendJsonToPi] Failed to create socket\n");
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    memcpy(&addr.sin_addr, he->h_addr, (size_t)he->h_length);
    addr.sin_port = htons((unsigned short)piPort);

    printf("[SendJsonToPi] Connecting to %s:%d\n", piHost, piPort);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        printf("[SendJsonToPi] Connection failed\n");
        close(fd);
        return;
    }

    printf("[SendJsonToPi] Sending: %s\n", json_payload);

    /* Send the HTTP request. */
    if (write(fd, http_request, (size_t)n) < 0) {
        printf("[SendJsonToPi] Write failed\n");
        close(fd);
        return;
    }

    /* Read and print the HTTP response (best-effort). */
    n = read(fd, response, (int)sizeof(response) - 1);
    if (n > 0) {
        response[n] = '\0';
        printf("[SendJsonToPi] Response: %s\n", response);
    }

    close(fd);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void SendJsonToPi_str(const char *device, const char *sensor,
                      const char *data)
{
    char json[JSON_BUF_SIZE];
    snprintf(json, sizeof(json),
             "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":\"%s\"}",
             device, sensor, data);
    post_payload(json);
}

void SendJsonToPi_int(const char *device, const char *sensor, int data)
{
    char json[JSON_BUF_SIZE];
    snprintf(json, sizeof(json),
             "{\"Device\":\"%s\",\"Sensor\":\"%s\",\"Data\":%d}",
             device, sensor, data);
    post_payload(json);
}
