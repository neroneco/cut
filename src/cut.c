#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <features.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

static void* reader(void* arg);
static void* analyzer(void* arg);
static void* printer(void* arg);

static void handler(int num);

enum THREAD {
    READER = 0,
    ANALYZER = 1,
    PRINTER = 2
};

static pthread_t thr[3];
static pthread_mutex_t mtx[2] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
static pthread_cond_t cond_var[2] = {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};

static int var[2];

// watchdog
int main() {

    size_t status[3];

    struct sigaction sa;
    sa.sa_handler = handler;
    sigaction(SIGTERM, &sa, NULL);

    pthread_create(&thr[READER], NULL, reader, &status[READER]);
    pthread_create(&thr[ANALYZER], NULL, analyzer, &status[ANALYZER]);
    pthread_create(&thr[PRINTER], NULL, printer, &status[PRINTER]);

    // TODO watchdog code

    pthread_join(thr[READER], NULL);
    pthread_join(thr[ANALYZER], NULL);
    pthread_join(thr[PRINTER], NULL);

    return 0;
}

void* reader(void* arg) {
    int tmp;
    size_t* status = (size_t*)arg;

    // FIXME change to infinite loop
    for (size_t i = 0; i < 20; i++) {
        status[READER] = 1;
        pthread_mutex_lock(&mtx[READER]);

        // TODO reader code
        var[0]++;
        tmp = var[0];

        pthread_cond_signal(&cond_var[READER]);
        pthread_mutex_unlock(&mtx[READER]);

        // FIXME only for testing
        printf("%zu reader var[0]: %i\n", i, tmp);
        usleep(3000000);
    }
    return 0;
}

static void* analyzer(void* arg) {
    int tmp;
    struct timespec ts;
    struct timeval  tv;

    size_t* status = (size_t*)arg;

    // FIXME change to infinite loop
    for(size_t i = 0; i < 20; i++) {
        status[ANALYZER] = 1;
        pthread_mutex_lock(&mtx[READER]);
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 1l;
        ts.tv_nsec = (tv.tv_usec * 1000l);
        pthread_cond_timedwait(&cond_var[READER], &mtx[READER], &ts);

        // TODO reader-analyzer code
        var[0]++;
        tmp = var[0];

        pthread_mutex_unlock(&mtx[READER]);
        printf("%zu analyzer var[0]: %i\n", i, tmp);
        usleep(10000);

        pthread_mutex_lock(&mtx[ANALYZER]);

        // TODO analyzer-printer code
        var[1]++;
        tmp = var[1];

        pthread_cond_signal(&cond_var[ANALYZER]);
        pthread_mutex_unlock(&mtx[ANALYZER]);

        // FIXME only for testing
        printf("%zu analyzer var[1]: %i\n", i, tmp);
        usleep(5000);
    }
    return 0;
}

static void* printer(void* arg) {
    int tmp;
    struct timespec ts;
    struct timeval  tv;

    size_t* status = (size_t*)arg;

    // FIXME change to infinite loop
    for(size_t i = 0; i < 20; i++) {
        status[PRINTER] = 1;
        pthread_mutex_lock(&mtx[ANALYZER]);
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 1l;
        ts.tv_nsec = (tv.tv_usec * 1000l);
        pthread_cond_timedwait(&cond_var[ANALYZER], &mtx[ANALYZER], &ts);

        // TODO printer code
        var[1]++;
        tmp = var[1];

        pthread_mutex_unlock(&mtx[ANALYZER]);

        // FIXME only for testing
        printf("%zu printer var[1]: %i\n", i, tmp);
    }
    return 0;
}

static void handler(int sig) {
    sig++;
    write(1, "SIGTERM", 7);
    // TODO cancle all thread, close file descriptor, free memory
}
