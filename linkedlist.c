#include "linkedlist.h"

Linkedlist *createLinkedlist() {
    Linkedlist *list = (Linkedlist *)malloc(sizeof(Linkedlist));
    if (list == NULL) return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    return list;
}

Node *newNode(void *generic) {
    Node *pnt = (Node *)malloc(sizeof(Node));
    if (pnt == NULL) return NULL;
    pnt->n = generic;
    pnt->next = NULL;
    return pnt;
}

void insertLast(Linkedlist *list, void * generic){
	Node *node = newNode(generic);
	if(list->tail != NULL){
		list->tail->next = node;
	}
	list->tail = node;
	if(list->length == 0){
		list->head = node;
	}
	list->length++;
}

void *getFromList(Linkedlist *list, int index){ 
	Node *extracted = extract(list, index);
	if(extracted != NULL){
		void *value = extracted->n;
		free(extracted);
		return value;
	}
	return NULL;
}

Node *extract(Linkedlist *list, int index) {
    if (list == NULL || list->length == 0 || index >= list->length || index < 0) {
        return NULL;
    }

    Node *previous = NULL;
    Node *toRemove = list->head;

    if (index == 0) {
        if (list->head == NULL) {  
            return NULL;
        }
        list->head = list->head->next;
        if (list->head == NULL) {
            list->tail = NULL;
        }
    } else {
        for (int i = 1; i <= index; i++) {
            previous = toRemove;
            toRemove = toRemove->next;
        }
        previous->next = toRemove->next;
        if (toRemove == list->tail) {
            list->tail = previous;
        }
    }

    list->length--;
    toRemove->next = NULL;
    return toRemove;
}

void delete(Linkedlist *list) {  
	Node *current = list->head;
	while(current != NULL){
		Node *next = current->next;
		free(current);
		current = next;
	}
	free(list);
}