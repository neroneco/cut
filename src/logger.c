#include "logger.h"
#include "queue.h"

void log_message(struct logger_t* log_queue, const char* str, ...) {
    char str_msg[200] = {0};
    char str_time[30] = {0};
    char str_log[300] = {0};
    size_t log_len = 0;
    time_t t = 0;
    struct tm *tmp;

    va_list args;
    va_start(args, str);
    vsprintf(str_msg, str, args);
    va_end(args);

    pthread_mutex_lock(log_queue->mtx);
    time(&t);
    tmp = localtime(&t);
    strftime(str_time, sizeof(str_time), "%d/%m/%Y %H:%M:%S %Z", tmp);
    sprintf(str_log, "%s\t%s\n ", str_time, str_msg);
    log_len = strlen(str_log);
    str_log[log_len-1] = '\0';
    enqueue(log_queue->q, str_log, log_len);
    pthread_mutex_unlock(log_queue->mtx);
}

void log_direct_message(FILE* stream, const char* str, ...) {
    char str_msg[200] = {0};
    char str_time[30] = {0};
    time_t t = 0;
    struct tm *tmp;

    va_list args;
    va_start(args, str);
    vsprintf(str_msg, str, args);
    va_end(args);

    time(&t);
    tmp = localtime(&t);
    strftime(str_time, sizeof(str_time), "%d/%m/%Y %H:%M:%S %Z", tmp);
    fprintf(stream, "%s\t%s\n ", str_time, str_msg);
}
