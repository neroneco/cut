#include <stdio.h>
#include <assert.h>

#include "../src/queue.h"

#define MAX_QUEUE_SIZE 8
#define MAX_ELEM_SIZE 30

static char data[9][40] = {
                        "First_data_in_Queue",
                        "aaa bbb ccc",
                        "hello 789",
                        "222333111",
                        "mkmkmkmkm",
                        "0x410x410x410x410x41",
                        "AAAAAAAAAAAAAAA",
                        "Last_data_in_Queue",
                        "Too_Big_Data_Here_more_then_30_chars"
                         };

int main() {

    struct queue q;
    char* deq_data;
    size_t len;

    printf("\nTEST Queues STARTED\n");

    init_queue(&q, MAX_QUEUE_SIZE, MAX_ELEM_SIZE);

    // enqueue tests
    for (size_t i = 0; i < MAX_QUEUE_SIZE - 1; i++) {
        // enqueue data
        assert(enqueue(&q, data[i], strlen(data[i])) == 0);
        // check size of enqueued data
        assert(q.tail->data_size == strlen(data[i]));
        // checkt size of queue
        assert(q.size == i + 1);
    }
    //try to enqueue too big data
    assert(enqueue(&q, data[MAX_QUEUE_SIZE], strlen(data[MAX_QUEUE_SIZE])) == 1);
    // try to enqueue to full queue
    assert(enqueue(&q, data[MAX_QUEUE_SIZE - 1], strlen(data[MAX_QUEUE_SIZE - 1])) == 0);
    assert(enqueue(&q, data[MAX_QUEUE_SIZE - 1], strlen(data[MAX_QUEUE_SIZE - 1])) == 1);

    // dequeue tests
    for (size_t i = 0; i < MAX_QUEUE_SIZE; i++) {
        deq_data = calloc(q.head->data_size,1);
        len = q.head->data_size;
        // checkt size of queue
        assert(q.size == MAX_QUEUE_SIZE - i );
        // dequeue data
        assert(dequeue(&q, deq_data) == 0);
        // check if dequeued data is the same
        assert(strncmp(data[i], deq_data, len) == 0);
        // check size of dequeued data
        assert(strlen(deq_data) == strlen(data[i]));
        free(deq_data);
    }
    // try to dequeue from empty queue
    deq_data = calloc(100,1);
    assert(dequeue(&q, deq_data) == 1);
    assert(q.size == 0);
    free(deq_data);

    deinit_queue(&q);

    printf("TEST Queues PASSED\n");

    return 0;
}
