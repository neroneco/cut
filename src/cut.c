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

#include "queue.h"

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

static struct queue q[2];

static int fd;

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

    // queue init
    init_queue(&q[READER], 100, 3000);
    init_queue(&q[ANALYZER], 100, 3000);

    // changing terminal mode (input available immediately)
    tcgetattr(STDIN_FILENO, &old_settings);
    memcpy(&new_settings, &old_settings, sizeof(struct termios));
    new_settings.c_lflag -= ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    // seting signals handler
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // creating threads
    pthread_create(&thr[READER], NULL, reader, &status[READER]);
    pthread_create(&thr[ANALYZER], NULL, analyzer, &status[ANALYZER]);
    pthread_create(&thr[PRINTER], NULL, printer, &status[PRINTER]);
    // TODO watchdog code
    for (size_t i = 1; !end[WATCHDOG] && !(c == 'q'); i++) {
        c = getc(stdin);
        usleep(1000000u);
        printf("Watchdog\n");
        i %= 20;
        if (!i) {
            if (!status[READER]) {
                printf("Reader hanging\n");
                end[WATCHDOG] = 1;
            }
            if (!status[ANALYZER]) {
                printf("Analyzer hanging\n");
                end[WATCHDOG] = 1;
            }
            if (!status[PRINTER]) {
                printf("Printer hanging\n");
                end[WATCHDOG] = 1;
            }
            status[READER] = 0;
            status[ANALYZER] = 0;
            status[PRINTER] = 0;
        }
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
    close(fd);

    return 0;
}



void* reader(void* arg) {
    size_t* status = (size_t*)arg;

    int rs;
    ssize_t data_size;
    char data_buffer[4096] = {0};
    fd = open("/proc/stat", O_RDONLY);
    if (fd == -1)
        pthread_exit(status);

    while (!end[READER]) {
        status[READER] = 1;

        // TODO read data
        data_size = read(fd, data_buffer, 4000);
        if (data_size == -1)
            pthread_exit(status);
        printf("Reader: %s\n", data_buffer);

        pthread_mutex_lock(&mtx[READER]);

        // TODO enqueue data for analyzer
        rs = enqueue(&q[READER], data_buffer, (size_t)data_size);

        pthread_cond_signal(&cond_var[READER]);
        pthread_mutex_unlock(&mtx[READER]);


        if (!rs) {
            printf("Reader: Data added to queue\n");
        } else {
            printf("Reader: No data added\n");
        }

        // FIXME only for testing
        usleep(1000000);
    }
    pthread_exit(status);
}



static void* analyzer(void* arg) {
    struct timespec ts;
    struct timeval  tv;

    size_t* status = (size_t*)arg;

    int rs = 1;
    ssize_t data_size = 0;
    char data_buffer[4096] = {0};

    while (!end[ANALYZER]) {
        status[ANALYZER] = 1;
        pthread_mutex_lock(&mtx[READER]);

        // Wait for signal but with 1 sec timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 1l;
        ts.tv_nsec = (tv.tv_usec * 1000l);
        pthread_cond_timedwait(&cond_var[READER], &mtx[READER], &ts);

        // TODO dequeue data from reader
        if (q[READER].size > 0) {
            data_size = (ssize_t)q[READER].head->data_size;
            rs = dequeue(&q[READER], data_buffer);
        } else {
            rs = 1;
        }
        pthread_mutex_unlock(&mtx[READER]);

        // TODO analyze data

        if (!rs) {
            printf("Analyzer: %s\n", data_buffer);
        } else {
            printf("Analyzer: No data in Readers queue\n");
        }



        pthread_mutex_lock(&mtx[ANALYZER]);

        // TODO enqueue data for printer
        rs = enqueue(&q[ANALYZER], data_buffer, (size_t)data_size);

        pthread_cond_signal(&cond_var[ANALYZER]);
        pthread_mutex_unlock(&mtx[ANALYZER]);

        if (!rs) {
            printf("Analyzer: data added to printer queue %zd \n", data_size);
        } else {
            printf("Analyzer: No data added\n");
        }

        // FIXME only for testing

    }
    pthread_exit(status);
}



static void* printer(void* arg) {
    struct timespec ts;
    struct timeval  tv;

    size_t* status = (size_t*)arg;

    int rs = 1;
    ssize_t data_size = 0;
    char data_buffer[4096] = {0};

    while (!end[PRINTER]) {
        status[PRINTER] = 1;
        pthread_mutex_lock(&mtx[ANALYZER]);

        // Wait for signal but with 1 sec timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec + 1l;
        ts.tv_nsec = (tv.tv_usec * 1000l);
        pthread_cond_timedwait(&cond_var[ANALYZER], &mtx[ANALYZER], &ts);

        // TODO dequeue data from analyzer
        if (q[ANALYZER].size > 0) {
            data_size = (ssize_t)q[ANALYZER].head->data_size;
            rs = dequeue(&q[ANALYZER], data_buffer);
        } else {
            rs = 1;
        }

        pthread_mutex_unlock(&mtx[ANALYZER]);

        // TODO print data to stdin
        if (!rs) {
            printf("Printer: %s\n", data_buffer);
        } else {
            printf("Printer: No data in queue\n");
        }
    }
    pthread_exit(status);
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
}
