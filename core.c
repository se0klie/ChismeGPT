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
sem_t prioritizerSem;

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
 
// void* processRequirement(void *args) {
//     pthread_detach(pthread_self());
//     int selfIndex = *(int *) args;
//     free(args);
//     sem_t *mutex = &semArray[selfIndex]; 
//     User *user;
//     char *buf = malloc(MAXLINE);
//     if (buf == NULL) {
//         perror("malloc failed");
//         return NULL;
//     }

//     while (1) { 
//         sem_wait(&mutex); 
//         user = execUsers[selfIndex];  

//         if (user != NULL) {
//             // if (!user->isClosed) {  
//             //     printf("Processing user ID %d with priority %d and message: %s\n", user->id, user->priority, user->req);
//             //     int n = read(user->connfd,buf,sizeof(buf));
//             //     if(n>0){
//             //         buf[n] = '\0';
//             //         user->req = buf;
//             //     }

//             //     if (strcmp(user->req, "CHAO\n") == 0) {
//             //         user->isClosed = true;
//             //         printf("User ID %d closed.\n", user->id);
//             //         execUsers[selfIndex] = NULL;
//             //         free(user);
//             //         close(user->connfd);
//             //         user= NULL;
//             //     } else {
//             //         usleep(1000);  
//             //         write(user->connfd, "Processing...", strlen("Processing..."));
//             //         struct item_q *newUs = malloc(sizeof(struct item_q));
//             //         newUs->user = user;
//             //         if (!user->isClosed) {
//             //             if(newUs->user->priority == 60){
//             //                 sem_wait(&postClientsSem);
//             //                 TAILQ_INSERT_TAIL(&paidClients,newUs,entries);
//             //                 sem_post(&postClientsSem);
//             //             } else {
//             //                 sem_wait(&waitCLientsSem);
//             //                 TAILQ_INSERT_TAIL(&waitingUsers,newUs,entries);
//             //                 sem_post(&waitCLientsSem);
//             //             }
//             //         }
//             //         free(newUs);
//             //     }
//             // }
//             printf("USER ID %d PRIOR %d\n",user->id,user->priority);
//         }
//         sem_post(&mutex);  // Release the semaphore after processing
        
//     }
//     free(buf);
//     return NULL;
// }

void * prioritize() {
    struct item_q *user = NULL;

    while(1) {
        sem_wait(&postClientsSem);
        if(!TAILQ_EMPTY(&paidClients)) {
            user = TAILQ_FIRST(&paidClients);
            printf("User id %d\n", user->user->id);
            TAILQ_REMOVE(&paidClients, user, entries);
        }
        sem_post(&postClientsSem);

        if (user == NULL) {
            sem_wait(&waitCLientsSem);
            if (!TAILQ_EMPTY(&waitingUsers)) {
                user = TAILQ_FIRST(&waitingUsers);
                printf("User id %d\n", user->user->id);
                TAILQ_REMOVE(&waitingUsers, user, entries);
            }
            sem_post(&waitCLientsSem);
        }

        if (user == NULL) {
            sem_wait(&preClientsSem);
            if (!TAILQ_EMPTY(&unpaidClients)) {
                user = TAILQ_FIRST(&unpaidClients);
                printf("User id %d\n", user->user->id);
                TAILQ_REMOVE(&unpaidClients, user, entries);
            }
            sem_post(&preClientsSem);
        }

        if (user != NULL) {
            sem_wait(&nextClientSem); 
            if (readyUser == NULL || readyUser->id > 100) { 
                readyUser = user;
                user = NULL; // Nullify user since it has been moved to readyUser
                printf("id %d prior %d\n", readyUser->id, readyUser->priority);
                sem_post(&nextClientSem); // Release lock for readyUser
            } else {
                sem_post(&nextClientSem); // If readyUser is not NULL, release the lock
            }
        }
    }
}



void * prioritize() {
    struct item_q *user = NULL;
    while(1){
        sem_wait(&postClientsSem);
        if(!TAILQ_EMPTY(&paidClients)){
            user = TAILQ_FIRST(&paidClients);
            printf("User id %d\n",user->user->id);
            TAILQ_REMOVE(&paidClients,user,entries);
        }
        sem_post(&postClientsSem);
        if(user==NULL){
            sem_wait(&waitCLientsSem);
            if(!TAILQ_EMPTY(&waitingUsers)){
                user = TAILQ_FIRST(&waitingUsers);
                printf("User id %d\n",user->user->id);
                TAILQ_REMOVE(&waitingUsers,user,entries);
            }
            sem_post(&waitCLientsSem);
        }
        if(user ==NULL){
            sem_wait(&preClientsSem);
            if(!TAILQ_EMPTY(&unpaidClients)){
                user = TAILQ_FIRST(&unpaidClients);
                printf("User id %d\n",user->user->id);
                TAILQ_REMOVE(&unpaidClients,user,entries);
            }
            sem_post(&preClientsSem);
        }
        

        if(user != NULL){
            while(1){
                sem_wait(&nextClientSem);
                if(readyUser == NULL){
                    readyUser = user;
                    user = NULL;
                    printf("id %d prior %d\n",readyUser->id,readyUser->priority);
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
    sem_init(&prioritizerSem,0,1);
    
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
        // if (pthread_create(&threads[i], NULL, processRequirement, (void *)index) != 0) {
        //     perror("Failed to create thread");
        //     exit(EXIT_FAILURE);
        // }


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

        struct item_q *user = createUser(*connfd);
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
    }
    free(execUsers);
    printf("All threads completed\n");
    return 0;
}