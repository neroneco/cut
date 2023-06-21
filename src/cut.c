#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <features.h>
#include <termios.h>
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
    PRINTER = 2,
    WATCHDOG = 3,
    LOGGER = 4
};

static pthread_t thr[3];
static pthread_mutex_t mtx[2] = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_MUTEX_INITIALIZER};
static pthread_cond_t cond_var[2] = {PTHREAD_COND_INITIALIZER, PTHREAD_COND_INITIALIZER};

static int end[4];

// watchdog
int main() {

    int c = 0;
    struct termios old_settings = {0};
    struct termios new_settings = {0};

    // status of thread:
    //    1) thread set status to 1
    //    2) watchdog resetes status to 0 with 2 sec intervals
    //    3) if status 0 after 2 sec - thread hanged
    size_t status[3];

    struct sigaction sa = {0};
    sa.sa_handler = handler;

    // changing terminal mode (input available immediately)
    tcgetattr(STDIN_FILENO, &old_settings);
    memcpy(&new_settings, &old_settings, sizeof(struct termios));
    new_settings.c_lflag -= ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);

    // seting signals handler
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // creating threads
    pthread_create(&thr[READER], NULL, reader, &status[READER]);
    pthread_create(&thr[ANALYZER], NULL, analyzer, &status[ANALYZER]);
    pthread_create(&thr[PRINTER], NULL, printer, &status[PRINTER]);

    // TODO watchdog code
    while (!end[WATCHDOG] && !(c == 'q')) {
        c = getc(stdin);
        usleep(100000u);
    }

    // changing terminal mode back
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);
    if (c == 'q')
        printf("\nEnding program (user quit)\n");

    // Ending program (normally)
    end[READER] = 1;
    end[ANALYZER] = 1;
    end[PRINTER] = 1;
    pthread_join(thr[READER], NULL);
    pthread_join(thr[ANALYZER], NULL);
    pthread_join(thr[PRINTER], NULL);

    return 0;
}



void* reader(void* arg) {
    size_t* status = (size_t*)arg;

    while (!end[READER]) {
        status[READER] = 1;
        pthread_mutex_lock(&mtx[READER]);

        // TODO reader code
        // enqueue data for analyzer

        pthread_cond_signal(&cond_var[READER]);
        pthread_mutex_unlock(&mtx[READER]);

        // FIXME only for testing
        usleep(3000000);
    }
    return 0;
}



static void* analyzer(void* arg) {
    struct timespec ts;
    struct timeval  tv;

    size_t* status = (size_t*)arg;

    while (!end[ANALYZER]) {
        status[ANALYZER] = 1;
        pthread_mutex_lock(&mtx[READER]);

        // Wait for signal but with 1 sec timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 1l;
        ts.tv_nsec = (tv.tv_usec * 1000l);
        pthread_cond_timedwait(&cond_var[READER], &mtx[READER], &ts);

        // TODO reader-analyzer code
        // analyze data

        pthread_mutex_unlock(&mtx[READER]);

        usleep(10000);

        pthread_mutex_lock(&mtx[ANALYZER]);

        // TODO analyzer-printer code
        // enqueue data for printer

        pthread_cond_signal(&cond_var[ANALYZER]);
        pthread_mutex_unlock(&mtx[ANALYZER]);

        // FIXME only for testing
        usleep(5000);
    }
    return 0;
}



static void* printer(void* arg) {
    struct timespec ts;
    struct timeval  tv;

    size_t* status = (size_t*)arg;

    while (!end[PRINTER]) {
        status[PRINTER] = 1;
        pthread_mutex_lock(&mtx[ANALYZER]);

        // Wait for signal but with 1 sec timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 1l;
        ts.tv_nsec = (tv.tv_usec * 1000l);
        pthread_cond_timedwait(&cond_var[ANALYZER], &mtx[ANALYZER], &ts);

        // TODO printer code
        // print data to stdin

        pthread_mutex_unlock(&mtx[ANALYZER]);

        // FIXME only for testing
    }
    return 0;
}



static void handler(int sig) {
    if (sig == SIGINT)
        write(1, "\nEnding program (interrupted SIGINT)\n", 38);
    if (sig == SIGTERM)
        write(1, "\nEnding program (interrupted SIGTERM)\n", 39);

    // Ending program (via inetterupt)
    pthread_cancel(thr[READER]);
    pthread_cancel(thr[ANALYZER]);
    pthread_cancel(thr[PRINTER]);
    pthread_join(thr[READER], NULL);
    pthread_join(thr[ANALYZER], NULL);
    pthread_join(thr[PRINTER], NULL);
    end[WATCHDOG] = 1;
    // TODO cancle all thread, close file descriptor, free memory
}
