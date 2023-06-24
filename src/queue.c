#include "queue.h"

void init_queue(struct queue* q, size_t max_queue_size, size_t max_elem_size) {
    q->max_queue_size = max_queue_size;
    q->max_elem_size = max_elem_size;
    q->size = 0;
    q->head = NULL;
    q->tail = NULL;
}

int enqueue(struct queue* q, void* data, size_t data_size) {

    // create new node
    struct node* new_node;

    if (q->size == q->max_queue_size)
        return 1;
    if (data_size > q->max_elem_size)
        return 1;
    if (data_size <= 0)
        return 1;

    // allocate memory
    new_node = malloc(data_size + sizeof(struct node));

    if (new_node == NULL)
        return 1;

    // copy data to new node
    memcpy(&new_node->data, data, data_size);
    new_node->data_size = data_size;

    // add to queue
    if (q->size != 0)
        q->tail->next = new_node;

    q->tail = new_node;

    if (q->size == 0)
        q->head = new_node;
    
    q->size++;
    return 0;
}

int dequeue(struct queue* q, void* data) {

    struct node* old_head;

    // check size
    if (q->size == 0)
        return 1;
    if (q->head == NULL)
        return 1;

    // copy data to given pointer
    if (data != NULL)
        memcpy(data, &q->head->data, q->head->data_size);
    
    // remove head from the queue
    old_head = q->head;
    q->head = q->head->next;
    q->size--;
    free(old_head);

    return 0;
}

void deinit_queue(struct queue* q) {
    while (q->size > 0) {
        dequeue(q, NULL);
    }
}
