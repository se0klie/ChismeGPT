#include <getopt.h>
#include <limits.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <errno.h>
#include "common.h"

#define MAXLINE 1024

int clientfd;
bool hasClosed = false;
pthread_mutex_t close_mutex = PTHREAD_MUTEX_INITIALIZER;
char *header = {0};

int numMessages = 1; 
int messageInterval = 1; 

void *receiveMessages(void *arg) {
    char read_buffer[MAXLINE + 1];
    ssize_t n;

    while (1) {
        pthread_mutex_lock(&close_mutex);
        if (hasClosed) {
            pthread_mutex_unlock(&close_mutex);
            break;
        }
        pthread_mutex_unlock(&close_mutex);

        n = read(clientfd, read_buffer, MAXLINE - 1);

        if (n > 0) {
        } else if (n == 0) {
            // Server closed the connection
            printf("Server disconnected.\n");
            break;
        } else {
            perror("Error reading from server");
            break;
        }
    }

    return NULL;
}
void *sendMessages(void *arg) {
    char *message = "Automated message."; // Default message

    char *buffer = (char *)malloc(MAXLINE);
    sprintf(buffer,"%s, %s", header,message);

    // Send the specified number of messages with the given interval
    for (int i = 0; i < numMessages; i++) {
        // Write message to the server
        if (write(clientfd, buffer, strlen(buffer)) <= 0) {
            perror("Error writing to server");
            break;
        }
        printf("Sent message %d: %s\n", i + 1, buffer);

        // Wait for the specified interval before sending the next message
        sleep(messageInterval);
    }

    // After sending all the messages, send "CHAO" to indicate termination
    char *closeMessage = "CHAO\n";
    if (write(clientfd, closeMessage, strlen(closeMessage)) <= 0) {
        perror("Error writing close message to server");
    }
    printf("Sent close message 'CHAO'.\n");

    // Close the connection after sending the close message
    pthread_mutex_lock(&close_mutex);
    hasClosed = true;  // Mark the connection as closed
    close(clientfd);  // Close the socket
    pthread_mutex_unlock(&close_mutex);

    return NULL;
}

int main(int argc, char **argv) {
    int opt;
    char *hostname = "127.0.0.1";
    int port = 8080;

    // Parse command-line options
    while ((opt = getopt(argc, argv, "t:h")) != -1) {
        switch (opt) {
            case 't':
                if(strcmp(optarg,"POST")==0){
                    header = "POST";
                } else {
                    header="PRE";
                }
                break;
            case 'h':
            default:
                printf("Usage: %s -t [type] [hostname] [port]\n", argv[0]);
                return 1;
        }
    }

    // Open the connection to the server
    clientfd = open_clientfd(hostname, "8080");
    if (clientfd < 0) {
        connection_error(clientfd);
        return 1;
    }

    printf("Connected to %s on port %d.\n", hostname, port);
    printf("Sending %d messages with a %d second interval.\n", numMessages, messageInterval);

    pthread_t receive_thread, send_thread;

    if (pthread_create(&receive_thread, NULL, receiveMessages, NULL) != 0) {
        perror("Error creating receive thread");
        close(clientfd);
        return 1;
    }

    if (pthread_create(&send_thread, NULL, sendMessages, NULL) != 0) {
        perror("Error creating send thread");
        close(clientfd);
        return 1;
    }

    pthread_join(receive_thread, NULL);
    pthread_join(send_thread, NULL);

    pthread_mutex_destroy(&close_mutex);
    printf("Disconnecting...\n");

    return 0;
}
