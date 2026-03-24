/**
 * @file Pi-2c.c
 * @brief TCP client that sends JSON sensor data and parses a JSON response.
 *
 * Usage: ./Pi-2c <hostname> <port>
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
    exit(0);
}

/**
 * @brief Connect to a TCP server, send JSON payload, and print the response.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments. argv[1] is hostname, argv[2] is port.
 * @return 0 on normal termination.
 */
int main(int argc, char *argv[])
{
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[256];
    if (argc < 3) {
       fprintf(stderr,"usage %s hostname port\n", argv[0]);
       exit(0);
    }
    portno = atoi(argv[2]);
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
    serv_addr.sin_port = htons(portno);
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");
    
    // Create JSON message
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "name", "temperature_sensor");
    cJSON_AddNumberToObject(json, "value", 23.5);
    cJSON_AddStringToObject(json, "unit", "celsius");
    
    char *jsonString = cJSON_Print(json);
    printf("Sending JSON: %s\n", jsonString);
    
    n = write(sockfd, jsonString, strlen(jsonString));
    if (n < 0) 
         error("ERROR writing to socket");
    
    bzero(buffer,256);
    n = read(sockfd,buffer,255);
    if (n < 0) 
         error("ERROR reading from socket");
    
    // Parse JSON response
    cJSON *response = cJSON_Parse(buffer);
    if (response) {
        cJSON *status = cJSON_GetObjectItemCaseSensitive(response, "status");
        cJSON *message = cJSON_GetObjectItemCaseSensitive(response, "message");
        if (status && status->valuestring) {
            printf("Status: %s\n", status->valuestring);
        }
        if (message && message->valuestring) {
            printf("Server: %s\n", message->valuestring);
        }
        cJSON_Delete(response);
    } else {
        printf("Server response: %s\n", buffer);
    }
    
    // Cleanup
    free(jsonString);
    cJSON_Delete(json);
    close(sockfd);
    return 0;
}
