#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/queue.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/queue.h>
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
        sem_wait(&mutex); 
        user = execUsers[selfIndex];  

        if (user != NULL) {
            if (!user->isClosed) {  
                printf("Processing user ID %d with priority %d and message: %s\n", user->id, user->priority, user->req);
                int n = read(user->connfd,buf,sizeof(buf));
                if(n>0){
                    buf[n] = '\0';
                    user->req = buf;
                }

                // if (strstr(user->req, "CHAO\n") == 0) {
                //     user->isClosed = true;
                //     printf("User ID %d closed.\n", user->id);
                //     execUsers[selfIndex] = NULL;
                //     free(user);
                //     close(user->connfd);
                //     user= NULL;
                // } else {
                //     usleep(1000);  
                //     write(user->connfd, "Processing...", strlen("Processing..."));
                //     struct item_q *newUs = malloc(sizeof(struct item_q));
                //     newUs->user = user;
                //     newUs->user->req = NULL;
                //     if (!user->isClosed) {
                //         if(newUs->user->priority == 60){
                //             sem_wait(&postClientsSem);
                //             TAILQ_INSERT_TAIL(&paidClients,newUs,entries);
                //             sem_post(&postClientsSem);
                //         } else {
                //             sem_wait(&waitCLientsSem);
                //             TAILQ_INSERT_TAIL(&waitingUsers,newUs,entries);
                //             sem_post(&waitCLientsSem);
                //         }
                //     }
                //     free(newUs);
                // }
            }
        }
        // printf("Bloq");
        sem_post(&mutex);  
        
    }
    free(buf);
    return NULL;
}


void *schedule() {
    User *userToSchedule;
    struct item_q *user;
    int index = 0;

    while (1) {
        sem_wait(&nextClientSem);
        if(userToSchedule == NULL && readyUser != NULL){
            userToSchedule = readyUser;
            readyUser = NULL;
            printf("Scheduled User id %d with priority %d\n", userToSchedule->id, userToSchedule->priority);
        }
        sem_post(&nextClientSem);

        while (userToSchedule != NULL) {
            printf("User to schedule %d\n",userToSchedule->priority);
            int isBeingUsed = sem_trywait(&semArray[index]);

            if (isBeingUsed == -1) { //is already blocked
                if (execUsers[index] != NULL){
                    if(execUsers[index]->priority < userToSchedule->priority) {
                        printf("[SWAP MESSAGE]\n");
                        user = execUsers[index];
                        execUsers[index] = NULL;
                        execUsers[index] = userToSchedule;
                        userToSchedule = NULL;

                        sem_wait(&waitingUsers);
                        TAILQ_INSERT_TAIL(&waitingUsers, user, entries);
                        sem_post(&waitingUsers);
                        user = NULL;
                        printf("Finished swapping...\n");
                        printf("USER[i] id %d\n",execUsers[index]->id);
                    } 
                } else {
                    printf("Put in empty space.\n");
                        execUsers[index] = userToSchedule;
                        userToSchedule = NULL;
                }
                sem_post(&semArray[index]);
            } else {
                printf("Not blocked\n");
            }

            index = (index + 1) % maxUserThreads;
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
        sem_init(&semArray[i],0,1);
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