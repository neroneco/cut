#ifndef _QUEUE_H
#define _QUEUE_H 1

#include <stdlib.h>
#include <string.h>

struct node {
    struct node* next;
    size_t data_size;
    size_t data;
};

struct queue {
    size_t max_queue_size;
    size_t max_elem_size;
    size_t size;
    struct node* head;
    struct node* tail;
};

void init_queue(struct queue* q, size_t max_queue_size, size_t max_elem_size);

int enqueue(struct queue* q, void* data, size_t data_size);

int dequeue(struct queue* q, void* data, size_t* data_size);

void deinit_queue(struct queue* q);

#endif
