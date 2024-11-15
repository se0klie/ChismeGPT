#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
void runClient(char *message) {
    // Convert integers to strings
    char intervalStr[10];

    // Create an array of arguments for execvp
    char *buf[] = {
        "./client",
        "-t",
        message,
        NULL            // Null-terminate the array
    };

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {
        // This is the child process, execvp to run the client
        execvp(buf[0], buf);
        perror("execvp failed");  // If execvp fails
        exit(1);  // Exit child process if execvp fails
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <num_clients>\n", argv[0]);
        return 1;
    }

    int numClients = atoi(argv[1]);  // Number of clients to launch
    int frequency = 2;  // Frequency (in seconds) for each client to wait between messages

    for (int i = 0; i < numClients; i++) {
        printf("Starting client %d...\n", i + 1);
        if(i%2==0){
            runClient("POST");  // Call function to start client
        } else {
            runClient("PRE");  // Call function to start client
        }
    }

    // Optionally, wait for all child processes to complete (if you need to wait)
    for (int i = 0; i < numClients; i++) {
        wait(NULL);  // Wait for each child process to finish
    }

    return 0;
}
