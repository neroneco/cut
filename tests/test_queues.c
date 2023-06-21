#include <stdio.h>
#include <assert.h>
#include "../src/queue.h"

static char data[7][30] = {
                        "abcd 123 def",
                        "aaa bbb ccc",
                        "hello 789",
                        "222333111",
                        "mkmkmkmkm",
                        "0x410x410x410x410x41",
                        "AAAAAAAAAAAAAAA"
                         };


int main() {

    struct queue q;
    char* deq_data;
    size_t len;

    printf("\nTEST Queues STARTED\n");

    // display test data
    for (size_t i = 0; i < 7; i++)
        printf("data[%20s] strlen: %lu\n", data[i], strlen(data[i]));

    // enqueue and dequeue above data
    init_queue(&q, 10, 30);

    printf("Enqueueing data...\n");
    printf("Checking if enqueued data is the same...\n");
    for (size_t i = 0; i < 7; i++)
        assert(enqueue(&q, data[i], strlen(data[i])) == 0);

    printf("Dequeueing data and checking if are the same...\n");
    for (size_t i = 0; i < 7; i++) {
        deq_data = calloc(q.head->data_size,1);
        len = q.head->data_size;
        assert(dequeue(&q, deq_data) == 0);
        assert(strncmp(data[i], deq_data, len) == 0);
        free(deq_data);
    }

    deinit_queue(&q);

    printf("\nTEST Queues ENDED SUCCESS\n");

    return 0;
}
