/*
 * Unified socket program: acts as server or client depending on args.
 *
 * Server mode: ./socket <peer_listen_port> <wmos_port>
 *   - listens for peer on <peer_listen_port>
 *   - listens for WMos HTTP on <wmos_port>
 *
 * Client mode: ./socket <wmos_port> <peer_host> <peer_port>
 *   - binds WMos HTTP on <wmos_port>
 *   - connects to peer at <peer_host>:<peer_port>
 *
 * Message framing: newline-delimited compact JSON. Stdin accepts CSV
 * commands Device,Sensor,Data which are converted to JSON and forwarded.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <stdbool.h>
#include <ctype.h>
#include "cJSON.h"

#define HTTP_BUFFER_SIZE 4096

static int peer_fd = -1;
static pthread_mutex_t peer_mutex = PTHREAD_MUTEX_INITIALIZER;

static void error(const char *msg) { perror(msg); exit(1); }
static int read_line(int fd, char *buf, int size) {
    int i = 0; char c;
    while (i < size - 1) {
        int n = read(fd, &c, 1);
        if (n <= 0) return n;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

static int send_peer(const char *s) {
    int rc = 0; pthread_mutex_lock(&peer_mutex);
    if (peer_fd < 0) { pthread_mutex_unlock(&peer_mutex); return -1; }
    size_t len = strlen(s); size_t written = 0;
    while (written < len) {
        ssize_t w = write(peer_fd, s + written, len - written);
        if (w <= 0) { perror("ERROR writing to peer"); rc = -1; break; }
        written += (size_t)w;
    }
    if (rc == 0) {
        ssize_t w = write(peer_fd, "\n", 1);
        if (w <= 0) { perror("ERROR writing newline to peer"); rc = -1; }
    }
    pthread_mutex_unlock(&peer_mutex);
    return rc;
}

static char *trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
    return s;
}

static void print_generic(cJSON *json) {
    cJSON *device = cJSON_GetObjectItemCaseSensitive(json, "Device");
    if (device && (device->type & cJSON_String)) printf("  Device: %s\n", device->valuestring);
    else printf("  Device: (unknown)\n");
    cJSON *sensor = cJSON_GetObjectItemCaseSensitive(json, "Sensor");
    if (sensor && (sensor->type & cJSON_String)) printf("  Sensor : %s\n", sensor->valuestring);
    cJSON *data = cJSON_GetObjectItemCaseSensitive(json, "Data");
    if (data) {
        if (data->type & cJSON_Number) printf("  Data   : %g\n", data->valuedouble);
        else if (data->type & cJSON_String) printf("  Data   : %s\n", data->valuestring);
        else printf("  Data   : (unknown)\n");
    }
    bool has_extra = false; cJSON *item = json->child;
    while (item) {
        if (item->string && strcmp(item->string, "Device") != 0 && strcmp(item->string, "Sensor") != 0 && strcmp(item->string, "Data") != 0) { has_extra = true; break; }
        item = item->next;
    }
    if (has_extra) {
        item = json->child;
        while (item) {
            char *val = cJSON_PrintUnformatted(item);
            printf("    %s = %s\n", item->string ? item->string : "?", val ? val : "(null)");
            free(val);
            item = item->next;
        }
    }
}

/* Reader thread: receives lines from peer and prints block */
static void *peer_reader(void *arg) {
    (void)arg; char buf[4096];
    while (1) {
        int n = read_line(peer_fd, buf, sizeof(buf));
        if (n <= 0) { printf("[socket] Peer disconnected\n"); fflush(stdout); break; }
        printf("--- Received from peer (%d bytes) ---\n", n);
        cJSON *json = cJSON_Parse(buf);
        if (!json) { printf("  Failed to parse JSON: %s\n", buf); }
        else { print_generic(json);
            cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");
            if (!(status && (status->type & cJSON_String) && strcmp(status->valuestring, "ack") == 0)) {
                /* reply ack */
                send_peer("{\"status\":\"ack\",\"Device\":\"peer\"}");
            }
            cJSON_Delete(json);
        }
        printf("-------------------------------------\n"); fflush(stdout);
    }
    return NULL;
}

/* WMos HTTP helpers */
static const char *extract_http_body(const char *request) {
    const char *body = strstr(request, "\r\n\r\n"); if (body) return body+4;
    body = strstr(request, "\n\n"); if (body) return body+2; return request;
}
static void send_http_response(int sockfd, const char *json_response) {
    char header[256]; int len = (int)strlen(json_response);
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n", len);
    if (write(sockfd, header, strlen(header)) < 0) perror("WARNING write header");
    if (write(sockfd, json_response, len) < 0) perror("WARNING write body");
}

static void *wmos_client_thread(void *arg) {
    int client = *(int*)arg; free(arg);
    char buffer[HTTP_BUFFER_SIZE]; memset(buffer,0,sizeof(buffer));
    int n = read(client, buffer, HTTP_BUFFER_SIZE-1);
    if (n < 0) { perror("ERROR reading from WMos"); close(client); return NULL; }
    const char *body = extract_http_body(buffer);
    cJSON *json = cJSON_Parse(body);
    if (!json) { send_http_response(client, "{\"error\":\"Invalid JSON\"}"); }
    else {
        char *compact = cJSON_PrintUnformatted(json);
        if (compact) { send_peer(compact); free(compact); send_http_response(client, "{\"status\":\"ok\"}"); }
        cJSON_Delete(json);
    }
    close(client); return NULL;
}

/* Stdin thread: CSV -> JSON -> send_peer */
static void *stdin_thread(void *arg) {
    (void)arg; char line[1024];
    while (fgets(line, sizeof(line), stdin)) {
        size_t len = strlen(line); if (len>0 && line[len-1]=='\n') line[len-1]='\0';
        if (strlen(line)==0) continue;
        char *p = strchr(line, ',');
        if (p) {
            char *dev = strtok(line, ","); char *sen = strtok(NULL, ","); char *dat = strtok(NULL, ",");
            if (dev && sen && dat) {
                dev = trim(dev); sen = trim(sen); dat = trim(dat);
                cJSON *obj = cJSON_CreateObject(); cJSON_AddStringToObject(obj, "Device", dev); cJSON_AddStringToObject(obj, "Sensor", sen);
                char *endptr = NULL; double v = strtod(dat, &endptr);
                if (endptr && *endptr=='\0') cJSON_AddNumberToObject(obj, "Data", v); else cJSON_AddStringToObject(obj, "Data", dat);
                char *json_str = cJSON_PrintUnformatted(obj); cJSON_Delete(obj);
                if (json_str) { send_peer(json_str); printf("[socket] Sent JSON to peer: %s\n", json_str); free(json_str); }
                continue;
            }
        }
        if (send_peer(line)==0) printf("[socket] Sent to peer: %s\n", line);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3 && argc != 4) {
        fprintf(stderr, "Usage:\n  Server: %s <peer_listen_port> <wmos_port>\n  Client: %s <wmos_port> <peer_host> <peer_port>\n", argv[0], argv[0]);
        exit(1);
    }

    bool is_server = (argc == 3);
    int wmos_port, peer_listen_port;
    const char *peer_host = NULL; int peer_port = 0;

    if (is_server) {
        peer_listen_port = atoi(argv[1]); wmos_port = atoi(argv[2]);
    } else {
        wmos_port = atoi(argv[1]); peer_host = argv[2]; peer_port = atoi(argv[3]);
    }

    /* If server: listen for peer and accept one connection. If client: connect. */
    if (is_server) {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0); if (sockfd < 0) error("ERROR opening socket");
        int opt=1; setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        struct sockaddr_in serv; bzero(&serv,sizeof(serv)); serv.sin_family=AF_INET; serv.sin_addr.s_addr=INADDR_ANY; serv.sin_port=htons(peer_listen_port);
        if (bind(sockfd,(struct sockaddr*)&serv,sizeof(serv))<0) error("ERROR binding");
        listen(sockfd,1); printf("[socket] Listening for peer on port %d\n", peer_listen_port); fflush(stdout);
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        peer_fd = accept(sockfd,(struct sockaddr*)&cli,&clilen); if (peer_fd<0) error("ERROR accept");
        printf("[socket] Peer connected (full-duplex)\n"); fflush(stdout); close(sockfd);
    } else {
        struct hostent *server = gethostbyname(peer_host); if (!server) { fprintf(stderr, "ERROR: no such host '%s'\n", peer_host); exit(1); }
        peer_fd = socket(AF_INET, SOCK_STREAM, 0); if (peer_fd<0) error("ERROR opening socket");
        struct sockaddr_in addr; bzero(&addr,sizeof(addr)); addr.sin_family=AF_INET; bcopy(server->h_addr, &addr.sin_addr.s_addr, server->h_length); addr.sin_port=htons(peer_port);
        if (connect(peer_fd,(struct sockaddr*)&addr,sizeof(addr))<0) error("ERROR connecting to peer");
        printf("[socket] Connected to peer at %s:%d (full-duplex)\n", peer_host, peer_port); fflush(stdout);
    }

    /* Start peer reader thread */
    pthread_t reader; if (pthread_create(&reader,NULL,peer_reader,NULL)!=0) error("ERROR creating reader thread"); pthread_detach(reader);
    /* Start stdin thread */
    pthread_t stdin_thr; if (pthread_create(&stdin_thr,NULL,stdin_thread,NULL)!=0) error("ERROR creating stdin thread"); pthread_detach(stdin_thr);

    /* Bind WMos HTTP port */
    int wmos_fd = socket(AF_INET, SOCK_STREAM, 0); if (wmos_fd<0) error("ERROR opening WMos socket");
    int opt = 1; setsockopt(wmos_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in waddr; bzero(&waddr,sizeof(waddr)); waddr.sin_family=AF_INET; waddr.sin_addr.s_addr=INADDR_ANY; waddr.sin_port=htons(wmos_port);
    if (bind(wmos_fd,(struct sockaddr*)&waddr,sizeof(waddr))<0) error("ERROR on bind WMos");
    listen(wmos_fd,5); printf("[socket] Listening for WMos on port %d\n", wmos_port); fflush(stdout);

    while (1) {
        struct sockaddr_in cli; socklen_t clilen = sizeof(cli);
        int client = accept(wmos_fd,(struct sockaddr*)&cli,&clilen);
        if (client < 0) { perror("ERROR on accept"); continue; }
        int *arg = malloc(sizeof(int)); *arg = client; pthread_t wt; if (pthread_create(&wt,NULL,wmos_client_thread,arg)!=0) { perror("ERROR creating WMos thread"); close(client); free(arg); continue; }
        pthread_detach(wt);
    }

    close(wmos_fd); close(peer_fd);
    return 0;
}
