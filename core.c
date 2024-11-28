#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <semaphore.h>
#include "linkedlist.h"
#include "common.h"

#define MAXTHREADS 10
typedef struct User {
    int connfd;
    int id;
    int priority;
    int messageQuan;
    bool isClosed;
    char *req;
    int messagesLeft;
} User;

Linkedlist *prepaidClientsQueue;
Linkedlist *postpaidClientsQueue;


int actualClientsInt = 0;
int maxUserThreads = 1;
int idClient = 0;

sem_t scheduleSem;
sem_t postClientsSem;
sem_t preClientsSem;

void* prioritize();
void separar_tokens(char *linea, char *delim, char *tokens[2]);
User * createUser(int connfd);
void* processRequirement(void *args);
void print_help(char *command);

int main(int argc, char **argv) {
    int listenfd;
    struct sockaddr_in clientaddr;
    int port = 8080;
    unsigned int clientlen;

    if(argc != 2){
        print_help(argv[0]);
        return 1;
    }

    sem_init(&postClientsSem,0,1);
    sem_init(&preClientsSem,0,1);
    sem_init(&scheduleSem,0,1);

    prepaidClientsQueue = createLinkedlist();
    postpaidClientsQueue = createLinkedlist();

    pthread_t threads[MAXTHREADS];
    pthread_t logic_threads[2];
    if(argc >1){
        if(atoi(argv[1])>0){
            maxUserThreads = atoi(argv[1]);
        } else {
            printf("Numero de mensajes concurrentes invalido. Inicializando con 1.\n");
        }
    }

    listenfd = open_listenfd("8080");
    if (listenfd < 0) {
        connection_error(listenfd);
    }

    printf("Server listening on port %d...\n", port);

    for (int i = 0; i < maxUserThreads; i++) {
        int *index = malloc(sizeof(int));
        *index = i;
        
        if (pthread_create(&threads[i], NULL, processRequirement, (void *)index) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }


    }
    
    while (1) {
        clientlen = sizeof(clientaddr);
        int *connfd = malloc(sizeof(int));
        
        if (connfd == NULL) {
            perror("malloc failed for connfd");
            continue;
        }

        *connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        if (*connfd < 0) {
            perror("accept failed");
            free(connfd);
            continue;
        }

        User * user = createUser(*connfd);
        if (user == NULL) {
            perror("Failed to create user");
            free(connfd);
            continue; 
        }

        if(user->priority==60){
            sem_wait(&postClientsSem);
            insertLast(postpaidClientsQueue,(void *)user);
            sem_post(&postClientsSem);
        } else {
            sem_wait(&preClientsSem);
            insertLast(prepaidClientsQueue,(void *)user);
            sem_post(&preClientsSem);
        }
        printf("New connection FD %d added to the queue.\n", *connfd);
    }

    printf("All threads completed\n");
    return 0;
}

void print_help(char *command)
{
	printf("uso:\n %s [-d] <numero_mensajes>\n", command);
	printf(" %s -h\n", command);
	printf("Opciones:\n");
	printf(" -h\t\t\tAyuda, muestra este mensaje\n");
	printf(" <numero_mensajes>\t\t\tNumero de mensajes recurrentes\n");
}

void separar_tokens(char *linea, char *delim, char *tokens[2])
{
    char *token;
    int i = 0;

    token = strtok(linea, delim);

    while (token != NULL && i < 2) {
        tokens[i] = token; 
        i++;
        token = strtok(NULL, delim);
    }

    while (i < 2) {
        tokens[i] = NULL;
        i++;
    }
}

User * createUser(int connfd) {
    User *newUser = malloc(sizeof(struct User));  
    if (newUser == NULL) {
        perror("malloc failed for newUser");
        return NULL;
    }
    
    newUser->id = idClient;
    idClient++;
    newUser->connfd = connfd;
    
    char *buffer = (char *)malloc(MAXLINE);
    if (buffer == NULL) {
        perror("malloc failed for buffer");
        free(newUser);
        return NULL;
    }

    int n = read(newUser->connfd, buffer, MAXLINE - 1);
    if (n < 0) {
        perror("Error reading from socket");
        free(buffer);
        free(newUser);
        return NULL;
    }
    
    buffer[n] = '\0';
    if (strstr(buffer, "POST") != NULL) {
        newUser->priority = 60;
    } else {
        newUser->priority = 20;
    }

    char *bufSplit[2];
    separar_tokens(buffer,"|",bufSplit);
    
    newUser->req = bufSplit[1];
    newUser->messagesLeft = atoi(bufSplit[0]);

    newUser->messageQuan = 10;
    newUser->isClosed = false;

    if (newUser->priority == 60) {
        write(newUser->connfd, "60", strlen("60"));
    } else {
        write(newUser->connfd, "20", strlen("20"));
    }
    free(buffer);
    return newUser;  
}

void* processRequirement(void *args) {
    pthread_detach(pthread_self());
    int selfIndex = *(int *) args;
    free(args);
    
    User *user = NULL; //si no es nulo significa q no es postpago

    while (1) { 
        
        if(user !=NULL){
            sem_wait(&postClientsSem);
            if(user->priority == 20 && postpaidClientsQueue->length>0){
                user = (User *)getFromList(postpaidClientsQueue,0);
            }
            sem_post(&postClientsSem);
        } else {
            sem_wait(&postClientsSem);
            if(postpaidClientsQueue->length>0){
                user = (User*)getFromList(postpaidClientsQueue,0);
            }
            sem_post(&postClientsSem);

            sem_wait(&preClientsSem);
            if(user == NULL && prepaidClientsQueue->length >0){
                user = (User*)getFromList(prepaidClientsQueue,0);
            }
            sem_post(&preClientsSem);
        }

        if(user!=NULL){
            if(!user->isClosed){
                user->messagesLeft--;

                if(user->priority == 20){
                    user->messageQuan--;
                    if(user->messageQuan == 0){
                        printf("[LIMIT!] User id %d reached maximum messages sent.\n",user->id);
                        if(user->id % 2 == 0){
                            printf("[UPGRADE!] User ID %d has been upgraded to premium\n",user->id);
                            user->priority = 60;
                        }
                         else {
                            user->isClosed = true;
                        }
                    }
                }

                if(user->messagesLeft == 0){
                    user->isClosed = true;
                }

                usleep(1000);
            } else {
                if(user->messagesLeft>0){
                    printf("[CLOSED!] User id %d, FD: %d, priority %d has finished with %d messages left.\n",user->id,user->connfd ,user->priority, user->messagesLeft);

                } else {
                    printf("[SUCCESS] User id %d, FD: %d, and priority %d has finished processing succesfully.\n",user->id, user->connfd,user->priority);
                }
                close(user->connfd);
                user = NULL;
            }
        }
    }
    return NULL;
}