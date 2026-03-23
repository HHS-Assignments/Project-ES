/**
 * @file Pi-1s.c
 * @brief TCP server that receives JSON, parses it, and returns a JSON response.
 *
 * Usage: ./Pi-1s <port>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

/**
 * @brief Start the TCP server, process one client message, and send a response.
 * @param argc Number of command-line arguments.
 * @param argv Command-line arguments. argv[1] must be the TCP port.
 * @return 0 on normal termination.
 */
int main(int argc, char *argv[])
{
     int sockfd, newsockfd, portno;
     socklen_t clilen;
     char buffer[256];
     struct sockaddr_in serv_addr, cli_addr;
     int n;
     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }
     sockfd = socket(AF_INET, SOCK_STREAM, 0);
     if (sockfd < 0) 
        error("ERROR opening socket");
     bzero((char *) &serv_addr, sizeof(serv_addr));
     portno = atoi(argv[1]);
     serv_addr.sin_family = AF_INET;
     serv_addr.sin_addr.s_addr = INADDR_ANY;
     serv_addr.sin_port = htons(portno);
     if (bind(sockfd, (struct sockaddr *) &serv_addr,
              sizeof(serv_addr)) < 0) 
              error("ERROR on binding");
     listen(sockfd,5);
     clilen = sizeof(cli_addr);
     newsockfd = accept(sockfd, 
                 (struct sockaddr *) &cli_addr, 
                 &clilen);
     if (newsockfd < 0) 
          error("ERROR on accept");
     bzero(buffer,256);
     n = read(newsockfd,buffer,255);
     if (n < 0) error("ERROR reading from socket");
     printf("Here is the message: %s\n",buffer);
     
     // Parse JSON from received data
     cJSON *json = cJSON_Parse(buffer);
     if (json == NULL) {
         printf("Failed to parse JSON\n");
         const char *errorMsg = "{\"error\":\"Invalid JSON\"};";
         write(newsockfd, errorMsg, strlen(errorMsg));
     } else {
         // Extract JSON fields
         cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
         cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");
         
         if (name && name->valuestring) {
             printf("Name: %s\n", name->valuestring);
         }
         if (value && value->type == cJSON_Number) {
             printf("Value: %.2f\n", value->valuedouble);
         }
         
         // Create JSON response
         cJSON *response = cJSON_CreateObject();
         cJSON_AddStringToObject(response, "status", "success");
         cJSON_AddStringToObject(response, "message", "I got your message");
         cJSON_AddNumberToObject(response, "received_bytes", n);
         
         char *jsonString = cJSON_Print(response);
         n = write(newsockfd, jsonString, strlen(jsonString));
         if (n < 0) error("ERROR writing to socket");
         
         // Cleanup
         free(jsonString);
         cJSON_Delete(response);
         cJSON_Delete(json);
     }
     close(newsockfd);
     close(sockfd);
     return 0; 
}
