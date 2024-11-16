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
Linkedlist *waitingClientsQueue; 

int actualClientsInt = 0;
int maxUserThreads = 0;
int idClient = 0;
User *readyUser;

sem_t *semArray;
sem_t postClientsSem;
sem_t preClientsSem;
sem_t waitCLientsSem;
sem_t nextClientSem;

User **execUsers;

//FUNCIONES
void* prioritize();

//lsof -t -i :8080 | xargs kill -9

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
    sem_t *mutex = &semArray[selfIndex]; 
    User *user = NULL;

    while (1) { 
        sem_wait(mutex); 
        user=execUsers[selfIndex];
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
                        } else {
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
                execUsers[selfIndex] = NULL;
                user = NULL;
            }
        }
        sem_post(mutex);  
    }

    return NULL;
}

 
void *schedule() {
    struct User *userToSchedule = NULL;
    struct User *prepaid;
    int index = 0;

    while (1) {
        sem_wait(&nextClientSem);
        userToSchedule = readyUser;
        readyUser = NULL;
        sem_post(&nextClientSem);

        while (userToSchedule != NULL) {
            int isBeingUsed = sem_trywait(&semArray[index]);
            if(isBeingUsed==0){
                if(execUsers[index] == NULL){
                    execUsers[index] = userToSchedule;
                    userToSchedule = NULL;
                    sem_post(&semArray[index]);
                    break;
                } else if(execUsers[index]->priority== 20 && userToSchedule->priority == 60){
                    
                    prepaid = execUsers[index];
                    execUsers[index] = userToSchedule;
                    userToSchedule = NULL;

                    sem_wait(&waitCLientsSem);
                    insertLast(waitingClientsQueue,(void*)prepaid);
                    prepaid = NULL;
                    sem_post(&waitCLientsSem);

                    sem_post(&semArray[index]);
                    break; 
                }
            }
            sem_post(&semArray[index]);

            index ++;
            if(index==maxUserThreads) index = 0;
        }
    }
}




void * prioritize() {
    struct User *user = NULL;
    while(1){
        sem_wait(&postClientsSem);
            if(postpaidClientsQueue->length>0){
                user = (User *)getFromList(postpaidClientsQueue,0);
            }
        sem_post(&postClientsSem);

        sem_wait(&waitCLientsSem);
        if(user==NULL){
            if(waitingClientsQueue->length > 0){
                user = (User *)getFromList(waitingClientsQueue,0);
            }
        }
        sem_post(&waitCLientsSem);
        
        sem_wait(&preClientsSem);
        if(user ==NULL){
            if(prepaidClientsQueue->length > 0){
                user = (User*)getFromList(prepaidClientsQueue,0);
            }
        }
        sem_post(&preClientsSem);
        

        if(user != NULL){
            while(1){
                sem_wait(&nextClientSem);
                if(readyUser == NULL){
                    readyUser = user;
                    user = NULL;
                    sem_post(&nextClientSem);
                    break;
                }
                sem_post(&nextClientSem);
            }
        }
    }
}

int main(int argc, char **argv) {
    int listenfd;
    struct sockaddr_in clientaddr;
    int port = 8080;
    unsigned int clientlen;

    sem_init(&postClientsSem,0,1);
    sem_init(&waitCLientsSem,0,1);
    sem_init(&preClientsSem,0,1);
    sem_init(&nextClientSem,0,1);
    
    prepaidClientsQueue = createLinkedlist();
    postpaidClientsQueue = createLinkedlist();
    waitingClientsQueue = createLinkedlist();

    pthread_t threads[MAXTHREADS];
    pthread_t logic_threads[2];
    maxUserThreads = (argc > 1) ? atoi(argv[1]) : MAXTHREADS;

    semArray = malloc(maxUserThreads * sizeof(sem_t));
    if (semArray == NULL) {
        perror("Failed to allocate memory");
        exit(1);
    }

    execUsers = malloc(maxUserThreads* sizeof(User *));
    if (execUsers == NULL) {
        perror("Failed to allocate memory");
        exit(1);
    }

    listenfd = open_listenfd("8080");
    if (listenfd < 0) {
        connection_error(listenfd);
    }

    printf("Server listening on port %d...\n", port);

    for (int i = 0; i < maxUserThreads; i++) {
        execUsers[i] = malloc(sizeof(struct User)); 
        int *index = malloc(sizeof(int));
        *index = i;
        if (execUsers[i] == NULL) {
            perror("Failed to allocate memory for execUsers[i]");
            exit(1);
        }
        if(sem_init(&semArray[i],0,1)<0){
            perror("sem");
            exit(1);
        }
        if (pthread_create(&threads[i], NULL, processRequirement, (void *)index) != 0) {
            perror("Failed to create thread");
            exit(EXIT_FAILURE);
        }
        execUsers[i] = NULL;


    }

    if (pthread_create(&logic_threads[0], NULL, prioritize, NULL) != 0) {
        perror("Failed to create prioritize thread");
        exit(EXIT_FAILURE);
    }
    if (pthread_create(&logic_threads[1], NULL, schedule, NULL) != 0) {
        perror("Failed to create prioritize thread");
        exit(EXIT_FAILURE);
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
    for (int i = 0; i < maxUserThreads; i++) {
        free(execUsers[i]);  
        sem_destroy(&semArray[i]);
    }
    free(execUsers);
    printf("All threads completed\n");
    return 0;
}