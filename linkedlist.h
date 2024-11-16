#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct node {
	void *n;
	struct node *next; 
}Node;

typedef struct Linkedlist {
	Node *head;
	Node *tail;
	int length;
}Linkedlist;

Linkedlist *createLinkedlist(); 
Node *newNode( void * ); 
void insertLast(Linkedlist *, void * ); 
void *getFromList(Linkedlist *, int );
Node *extract(Linkedlist * , int ); 
void delete(Linkedlist *);