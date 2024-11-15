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
bool hasClosed = false;  // Use atomic flag for simplicity
char *header = NULL;

int numMessages = 1;

void *receiveMessages(void *arg) {
    char read_buffer[MAXLINE + 1];
    ssize_t n;

    while (1) {
        n = read(clientfd, read_buffer, MAXLINE - 1);
        if (n > 0) {
            read_buffer[n] = '\0';
            printf("Received from server: %s\n", read_buffer);
        } else if (n == 0) {
            printf("Server disconnected.\n");
            break;
        } else if (errno == EBADF) {
            // Handle "Bad file descriptor" gracefully
            printf("Error: Bad file descriptor. Connection is likely closed.\n");
            break;
        } else if (errno != EINTR) {
            perror("Error reading from server");
            break;
        }
    }

    return NULL;
}


void *sendMessages(void *arg) {
    char *message = "Automated message."; // Default message
    char *buffer = malloc(MAXLINE);
    if (!buffer) {
        perror("Memory allocation failed");
        return NULL;
    }
    snprintf(buffer, MAXLINE, "%s, %s", header, message);

    for (int i = 0; i < numMessages; i++) {
        ssize_t len = strlen(buffer);
        if (write(clientfd, buffer, len) != len) {
            perror("Error writing to server");
            break;
        }
        printf("Sent message %d: %s\n", i + 1, buffer);
    }

    char *closeMessage = "CHAO\n";
    if (write(clientfd, closeMessage, strlen(closeMessage)) <= 0) {
        perror("Error writing close message to server");
    }
    printf("Sent close message 'CHAO'.\n");

    close(clientfd);  // Close the socket
    free(buffer);

    return NULL;
}

int main(int argc, char **argv) {
    int opt;
    char *hostname = "127.0.0.1";
    int port = 8080;

    while ((opt = getopt(argc, argv, "t:n:h")) != -1) {
        switch (opt) {
            case 't':
                header = (strcmp(optarg, "POST") == 0) ? "POST" : "PRE";
                break;
            case 'n':
                numMessages = atoi(optarg);
                if (numMessages <= 0) {
                    fprintf(stderr, "Invalid number of messages: %d\n", numMessages);
                    return 1;
                }
                break;
            case 'h':
            default:
                printf("Usage: %s -t [type] -n [numMessages] [hostname] [port]\n", argv[0]);
                return 1;
        }
    }

    clientfd = open_clientfd(hostname, "8080");
    if (clientfd < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to %s on port %d.\n", hostname, port);

    pthread_t receive_thread, send_thread;

    if (pthread_create(&receive_thread, NULL, receiveMessages, NULL) != 0) {
        perror("Error creating receive thread");
        close(clientfd);
        return 1;
    }

    if (pthread_create(&send_thread, NULL, sendMessages, NULL) != 0) {
        perror("Error creating send thread");
        close(clientfd);
        pthread_join(receive_thread, NULL);  // Wait for receive thread to exit
        return 1;
    }

    pthread_join(receive_thread, NULL);
    pthread_join(send_thread, NULL);

    printf("Disconnecting...\n");
    return 0;
}
