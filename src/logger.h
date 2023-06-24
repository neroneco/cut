#ifndef _LOGGER_H
#define _LOGGER_H 1

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <pthread.h>

struct logger_t {
    struct queue* q;
    pthread_mutex_t* mtx;
};

void log_message(struct logger_t* log_queue, const char* str, ...);

void log_direct_message(FILE* stream, const char* str, ...);

#endif
