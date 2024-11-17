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
        NULL          
    };

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork failed");
        exit(1);
    } else if (pid == 0) {

        execvp(buf[0], buf);
        perror("execvp failed"); 
        exit(1);  
    }
}

void print_help(char *command)
{
	printf("uso:\n %s <num_clientes> <num_mensajes> [-h]\n", command);
	printf(" %s -h\n", command);
	printf("Opciones:\n");
	printf(" -h\t\t\tAyuda, muestra este mensaje\n");
	printf("<num_clientes>\t\t\tNumero de clientes a generar.\n");
    printf("<num_mensajes>\t\t\tNumero de mensajes a enviar. Por defecto, 2.\n");
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_clients> <num_message_p_client>\n", argv[0]);
        return 1;
    }

    int numClients = atoi(argv[1]);  
    
    for (int i = 0; i < numClients; i++) {
        printf("Starting client %d...\n", i + 1);
        if(i%2==0){
            runClient("POST",argv[2]);  
        } else {
            runClient("PRE", argv[2]); 
        }
    }

    for (int i = 0; i < numClients; i++) {
        wait(NULL);  
    }


    return 0;
}
