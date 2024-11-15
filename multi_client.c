#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "common.h"
void runClient(char *message, char* numMessages) {

    // Create an array of arguments for execvp
    char *buf[] = {
        "./client",
        "-t",
        message,
        "-n",
        numMessages,
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
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_clients> <num_message_p_client>\n", argv[0]);
        return 1;
    }

    int numClients = atoi(argv[1]);  // Number of clients to launch

    for (int i = 0; i < numClients; i++) {
        printf("Starting client %d...\n", i + 1);
        if(i%2==0){
            runClient("POST",argv[2]);  // Call function to start client
        } else {
            runClient("PRE", argv[2]);  // Call function to start client
        }
    }

    // Optionally, wait for all child processes to complete (if you need to wait)
    for (int i = 0; i < numClients; i++) {
        wait(NULL);  // Wait for each child process to finish
    }

    return 0;
}
