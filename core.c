#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <semaphore.h>

#include "common.h"
#define MAXTHREADS 10
typedef struct User {
    int connfd;
    int id;
    int priority;
    int messageQuan;
    bool isClosed;
    char *req;
} User;

typedef struct item_q {
    User *user;
    TAILQ_ENTRY(item_q) entries;
} item_q;

typedef struct fdqueue {
    int fd;
    TAILQ_ENTRY(fdqueue) fds;
} fdqueue;
typedef TAILQ_HEAD(head_t, fdqueue) fdhead;
fdhead filesdes;

typedef TAILQ_HEAD(head_s, item_q) head_t;
head_t paidClients;
head_t unpaidClients;
head_t waitingUsers;


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

struct item_q* createUser(int connfd) {
    // Allocate memory for the item_q struct itself
    struct item_q *newUser = malloc(sizeof(struct item_q));  
    if (newUser == NULL) {
        perror("malloc failed for newUser");
        return NULL;
    }

    // Allocate memory for the user struct inside item_q
    newUser->user = malloc(sizeof(struct User));  
    if (newUser->user == NULL) {
        perror("malloc failed for newUser->user");
        free(newUser);
        return NULL;
    }
    newUser->user->id = idClient;
    idClient++;
    newUser->user->connfd = connfd;
    
    // Allocate memory for buffer
    char *buffer = (char *)malloc(MAXLINE);
    if (buffer == NULL) {
        perror("malloc failed for buffer");
        free(newUser->user);
        free(newUser);
        return NULL;
    }

    // Read data from the socket
    int n = read(newUser->user->connfd, buffer, MAXLINE - 1);
    if (n < 0) {
        perror("Error reading from socket");
        free(buffer);
        free(newUser->user);
        free(newUser);
        return NULL;
    }
    
    // Null-terminate and store the buffer in `req`
    buffer[n] = '\0';
    newUser->user->req = buffer;

    // Determine user priority based on content
    if (strstr(buffer, "POST") != NULL) {
        newUser->user->priority = 60;
    } else {
        newUser->user->priority = 20;
    }

    // Initialize other fields
    newUser->user->messageQuan = 0;
    newUser->user->isClosed = false;

    // Send priority info back to client
    if (newUser->user->priority == 60) {
        write(newUser->user->connfd, "60", strlen("60"));
    } else {
        write(newUser->user->connfd, "20", strlen("20"));
    }

    return newUser;  
}
 void* processRequirement(void *args) {
    pthread_detach(pthread_self());
    int selfIndex = *(int *) args;
    free(args);
    sem_t *mutex = &semArray[selfIndex]; 
    User *user = NULL;
    char *buf = malloc(MAXLINE);
    if (buf == NULL) {
        perror("malloc failed");
        return NULL;
    }

    while (1) { 
        sem_wait(mutex);  // Wait for mutex
        
        user = execUsers[selfIndex];  

        if (user != NULL && user->req != NULL) {
            if (!user->isClosed) {  
                // if (strstr(user->req, "CHAO\n") != NULL) {
                //     user->isClosed = true;
                //     printf("User ID %d closed.\n", user->id);
                //     execUsers[selfIndex] = NULL;
                //     free(user);
                //     close(user->connfd);
                //     user = NULL;
                // } else {
                    usleep(1000);  // Some delay
                    write(user->connfd, "Processing...", strlen("Processing..."));

                    struct item_q *newUs = malloc(sizeof(struct item_q));
                    if (newUs == NULL) {
                        perror("malloc failed for newUs");
                        sem_post(mutex);
                        continue;
                    }

                    newUs->user = user;

                    if (user->isClosed == false) {
                        if (newUs->user->priority == 60) {
                            sem_wait(&postClientsSem);
                            TAILQ_INSERT_TAIL(&paidClients, newUs, entries);
                            sem_post(&postClientsSem);
                        } else {
                            sem_wait(&waitCLientsSem);
                            TAILQ_INSERT_TAIL(&waitingUsers, newUs, entries);
                            sem_post(&waitCLientsSem);
                        }
                    }

                    free(newUs);  // Free the allocated memory for newUs
                
            }
        }
        sem_post(mutex);  // Release mutex
    }

    free(buf);  // Free buffer
    return NULL;
}

 
void *schedule() {
    User *userToSchedule = NULL;
    struct item_q *swapUser;
    int index = 0;

    while (1) {
        sem_wait(&nextClientSem);
        userToSchedule = readyUser;
        readyUser = NULL;
        sem_post(&nextClientSem);

        while (userToSchedule != NULL) {
            printf("Sched user %d with priority %d and message %s\n",userToSchedule->id,userToSchedule->priority,userToSchedule->req);
            int isBeingUsed = sem_trywait(&semArray[index]);

            if (isBeingUsed == 0) { 
                if(execUsers[index] == NULL){
                    printf("Put in empty space.\n");
                    execUsers[index] = userToSchedule;
                    userToSchedule = NULL;
                } else if(execUsers[index]->priority < userToSchedule->priority){
                    swapUser = malloc(sizeof(struct item_q));
                    if(swapUser!=NULL){
                        swapUser->user = execUsers[index];
                        printf("[SWAP MESSAGE]\n");
                        execUsers[index] = userToSchedule;
                        userToSchedule = NULL;

                        sem_wait(&waitingUsers);
                        TAILQ_INSERT_TAIL(&waitingUsers, swapUser, entries);  // Insert the user into waiting queue
                        sem_post(&waitingUsers);

                        swapUser = NULL;  // Clear temporary user reference
                        printf("Finished swapping...\n");
                        printf("USER[i] id %d\n", execUsers[index]->id);
                    }
                }
                sem_post(&semArray[index]);
            }
            index++;
            if(index == maxUserThreads) index = 0;
        }
        index = 0;
    }
}




void * prioritize() {
    struct item_q *user = NULL;
    while(1){
        sem_wait(&postClientsSem);
        if(!TAILQ_EMPTY(&paidClients)){
            user = TAILQ_FIRST(&paidClients);
            TAILQ_REMOVE(&paidClients,user,entries);
        }
        sem_post(&postClientsSem);
        sem_wait(&waitCLientsSem);
        if(user==NULL){
            if(!TAILQ_EMPTY(&waitingUsers)){
                user = TAILQ_FIRST(&waitingUsers);
                TAILQ_REMOVE(&waitingUsers,user,entries);
            }
        }
        sem_post(&waitCLientsSem);
        
        sem_wait(&preClientsSem);
        if(user ==NULL){
            if(!TAILQ_EMPTY(&unpaidClients)){
                user = TAILQ_FIRST(&unpaidClients);
                TAILQ_REMOVE(&unpaidClients,user,entries);
            }
        }
        sem_post(&preClientsSem);
        

        if(user != NULL){
            while(1){
                sem_wait(&nextClientSem);
                if(readyUser == NULL){
                    readyUser = user->user;
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
    
    TAILQ_INIT(&paidClients);
    TAILQ_INIT(&unpaidClients);
    TAILQ_INIT(&waitingUsers);

    sem_init(&postClientsSem,0,1);
    sem_init(&waitCLientsSem,0,1);
    sem_init(&preClientsSem,0,1);
    sem_init(&nextClientSem,0,1);
    
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
    struct item_q * user;
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

        user = createUser(*connfd);
        if (user == NULL) {
            perror("Failed to create user");
            free(connfd);
            continue; 
        }

        if (user->user->priority == 60) {
            sem_wait(&postClientsSem);
            TAILQ_INSERT_TAIL(&paidClients, user, entries);
            sem_post(&postClientsSem);
        } else {
            sem_wait(&preClientsSem);
            TAILQ_INSERT_TAIL(&unpaidClients, user, entries);
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