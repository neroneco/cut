#define _DEFAULT_SOURCE
#include <features.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <termios.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>

#include "queue.h"
#include "analyzer.h"
#include "logger.h"


static void* reader(void* arg);
static void* analyzer(void* arg);
static void* printer(void* arg);
static void* logger(void* arg);

static void exit_handler(int num);

#define THREAD_POOL 10

// FIXME This numbers are wrong
enum THREAD {
    READER = 0,
    ANALYZER = 1,
    LOGGER = 2,
    PRINTER = 3,
    WATCHDOG = 4,
};

static pthread_t thr[THREAD_POOL];
static pthread_mutex_t mtx[3] = {PTHREAD_MUTEX_INITIALIZER,
                                 PTHREAD_MUTEX_INITIALIZER,
                                 PTHREAD_MUTEX_INITIALIZER};
static pthread_cond_t cond_var[3] = {PTHREAD_COND_INITIALIZER,
                                     PTHREAD_COND_INITIALIZER,
                                     PTHREAD_COND_INITIALIZER};

static int end[THREAD_POOL];

static struct queue q[3];

static int fd;
static FILE* log_stream;

static struct logger_t log_queue = {.q = &q[LOGGER],
                                    .mtx = &mtx[LOGGER]};

// watchdog
int main() {

    int c = 0;
    struct termios old_settings = {0};
    struct termios new_settings = {0};

    // status of thread:
    //    1) thread set status to 1
    //    2) watchdog resetes status to 0 with 2 sec intervals
    //    3) if status 0 after 2 sec - thread hanged
    size_t status[THREAD_POOL] = {0};

    struct sigaction sa = {0};
    sa.sa_handler = exit_handler;

    // queue init
    init_queue(&q[READER], 100, 3000);
    init_queue(&q[ANALYZER], 100, 3000);
    init_queue(&q[LOGGER], 100, 3000);

    // changing terminal mode (input available immediately)
    tcgetattr(STDIN_FILENO, &old_settings);
    memcpy(&new_settings, &old_settings, sizeof(struct termios));
    new_settings.c_lflag -= ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    // creating log file stream
    log_stream = fopen("log.txt", "a");

    // creating threads
    pthread_create(&thr[READER], NULL, reader, status);
    pthread_create(&thr[ANALYZER], NULL, analyzer, status);
    pthread_create(&thr[PRINTER], NULL, printer, status);
    pthread_create(&thr[LOGGER], NULL, logger, status);

    // seting signals handler
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // watchdog code; cyclic 2 sec check
    for (size_t i = 1; !end[WATCHDOG] && !(c == 'q'); i++) {
        c = getc(stdin);
        usleep(100000u);
        i %= 20;
        if (!i) {
            if (!status[READER]) {
                log_direct_message(log_stream, "[WATCHDOG] - Reader hanging. Ending...");
                end[WATCHDOG] = 1;
                break;
            }
            if (!status[ANALYZER]) {
                log_direct_message(log_stream, "[WATCHDOG] - Analyzer hanging. Ending...");
                end[WATCHDOG] = 1;
                break;
            }
            if (!status[PRINTER]) {
                log_direct_message(log_stream, "[WATCHDOG] - Printer hanging. Ending...");
                end[WATCHDOG] = 1;
                break;
            }
            // TODO add write to log file if logger hangs
            if (!status[LOGGER]) {
                log_direct_message(log_stream, "[WATCHDOG] - Logger hanging. Ending...");
                end[WATCHDOG] = 1;
                break;
            }
            log_message(&log_queue, "[WATCHDOG] - CHECK all thread OK");
            status[READER] = 0;
            status[ANALYZER] = 0;
            status[PRINTER] = 0;
            status[LOGGER] = 0;
        }
    }
    // changing terminal mode back
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

    // Ending program
    exit_handler(0);

    log_direct_message(log_stream, "[WATCHDOG] - END - closing files and freeing memory");

    // Cleanup
    close(fd);
    fclose(log_stream);
    deinit_queue(&q[READER]);
    deinit_queue(&q[ANALYZER]);
    deinit_queue(&q[LOGGER]);

    return 0;
}





void* reader(void* arg) {
    size_t* status = (size_t*)arg;

    int rs;
    ssize_t data_size;
    char data_buffer[4096] = {0};
    time_t t;

    while (!end[READER]) {
        status[READER] = 1ul;

        fd = open("/proc/stat", O_RDONLY);
        if (fd == -1) {
            log_message(&log_queue, "[READER] - Couldn't open /proc/stat");
            pthread_exit(status);
        }
        // read data
        data_size = read(fd, data_buffer, 4000);
        if (data_size == -1) {
            log_message(&log_queue, "[READER] - Couldn't read from /proc/stat");
            pthread_exit(status);
        }

        pthread_mutex_lock(&mtx[READER]);

        // enqueue data for analyzer
        rs = enqueue(&q[READER], data_buffer, (size_t)data_size);

        pthread_cond_signal(&cond_var[READER]);
        pthread_mutex_unlock(&mtx[READER]);


        if (!rs)
            log_message(&log_queue, "[READER] - Read %zu bytes and added to Queue (queue[READER].size = %zu)", data_size, q[READER].size);

        close(fd);
        // FIXME only for testing
        usleep(1000000);
    }
    pthread_exit(status);
}





static void* analyzer(void* arg) {
    struct timespec ts;
    struct timeval  tv;
    long timeout = 500l; // thread condition timeout in miliseconds

    size_t* status = (size_t*)arg;

    int rs = 1;
    ssize_t data_size = 0;
    char data_buffer_raw[4096] = {0};

    // I assume that there will not be more then 30 cores
    size_t cpus_num = 0;
    double total = 0;
    double idle = 0;
    double cpu_usage[30] = {0};
    struct cpu_time cpu_new_time[30] = {0};
    struct cpu_time cpu_old_time[30] = {0};

    while (!end[ANALYZER]) {
        status[ANALYZER] = 1ul;

        pthread_mutex_lock(&mtx[READER]);

        // Wait for signal but with 1 sec timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_nsec = (tv.tv_usec * 1000l) + timeout * 1000000; // 500 [ms]
        ts.tv_sec = tv.tv_sec + (ts.tv_nsec / 1000000000l);
        ts.tv_nsec %= 1000000000l;
        pthread_cond_timedwait(&cond_var[READER], &mtx[READER], &ts);

        // dequeue data from reader
        if (q[READER].size > 0) {
            data_size = (ssize_t)q[READER].head->data_size;
            rs = dequeue(&q[READER], data_buffer_raw);
        } else {
            rs = 1;
        }
        pthread_mutex_unlock(&mtx[READER]);

        // analyze data
        if (!rs) {
            log_message(&log_queue, "[ANALYZER] - Dequeued %zu bytes from Readers Queue", data_size);
        } else {
            continue;
        }

        // get number of cpus (cpus num is always too big by one)
        cpus_num = get_cpu_count(data_buffer_raw, data_size);
        if (cpus_num < 2)
            pthread_exit(status);

        // get new cpu idle/total time data and compare it with previous one 
        // after that determine cpu_usage
        analyze(cpu_new_time, cpus_num ,data_buffer_raw, data_size);
        for (size_t i = 0; i < cpus_num; i++) {
            idle = (double)(cpu_new_time[i].idle - cpu_old_time[i].idle);
            total = (double)(cpu_new_time[i].total - cpu_old_time[i].total);
            cpu_usage[i] = ((total - idle) / total) * 100.0;
        }
        memcpy(cpu_old_time, cpu_new_time, sizeof(struct cpu_time) * cpus_num);
        log_message(&log_queue, "[ANALYZER] - Data analyzed, defined cpu cores: %zu", cpus_num);

        pthread_mutex_lock(&mtx[ANALYZER]);

        // enqueue data for printer
        rs = enqueue(&q[ANALYZER], cpu_usage, (size_t)(cpus_num * sizeof(double)));

        pthread_cond_signal(&cond_var[ANALYZER]);
        pthread_mutex_unlock(&mtx[ANALYZER]);

        if (!rs)
            log_message(&log_queue, "[ANALYZER] - Added data to Printers Queue (queue[ANALYZER].size = %zu)", q[ANALYZER].size);

        // FIXME only for testing
        usleep(1000);
    }
    pthread_exit(status);
}





static void* printer(void* arg) {
    struct timespec ts;
    struct timeval  tv;
    long timeout = 500l; // thread condition timeout in miliseconds

    size_t* status = (size_t*)arg;

    int rs = 1;
    ssize_t data_size = 0;
    size_t cpus_num = 0;
    double cpu_usage[30] = {0};

    while (!end[PRINTER]) {
        status[PRINTER] = 1ul;

        pthread_mutex_lock(&mtx[ANALYZER]);

        // Wait for signal but with 500 ms timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_nsec = (tv.tv_usec * 1000l) + timeout * 1000000; // 500 [ms]
        ts.tv_sec = tv.tv_sec + (ts.tv_nsec / 1000000000l);
        ts.tv_nsec %= 1000000000l;
        pthread_cond_timedwait(&cond_var[ANALYZER], &mtx[ANALYZER], &ts);

        // dequeue data from analyzer
        if (q[ANALYZER].size > 0) {
            data_size = (ssize_t)q[ANALYZER].head->data_size;
            rs = dequeue(&q[ANALYZER], cpu_usage);
        } else {
            rs = 1;
        }

        pthread_mutex_unlock(&mtx[ANALYZER]);

        if (!rs) {
            log_message(&log_queue, "[PRINTER] - Dequeued %zu bytes from printers Queue", data_size);
        } else {
            continue;
        }

        // print data to stdin
        // TODO use esc char to make output more visible
        cpus_num = ((size_t)data_size / sizeof(double));
        for (size_t i = 0; i < cpus_num; i++) {
            if (!i)
                printf("\tcpu overal usage: %.3f%%\n", cpu_usage[i]);
            else
                printf("\tcpu%zu: %.3f%%\n", i, cpu_usage[i]);
        }
        // FIXME only for testing
        usleep(1000);
    }
    pthread_exit(status);
}





static void* logger(void* arg) {
    size_t* status = (size_t*)arg;
    struct timespec ts;
    struct timeval  tv;
    long timeout = 500l; // thread condition timeout in miliseconds

    int rs = 1;
    ssize_t data_size = 0;
    char message[300] = {0};

    while (!end[LOGGER]) {
        status[LOGGER] = 1;
        pthread_mutex_lock(&mtx[LOGGER]);

        while (q[LOGGER].size > 0) {
            data_size = (ssize_t)q[LOGGER].head->data_size;
            rs = dequeue(&q[LOGGER], message);
            fprintf(log_stream, "%s", message);
        }

        pthread_mutex_unlock(&mtx[LOGGER]);
        sleep(1);
    }
    pthread_exit(status);
}





static void exit_handler(int sig) {
    if (sig == SIGINT) {
        write(1, "\nEnding program (Interrupted SIGINT)\n", 38);
    } else if (sig == SIGTERM) {
        write(1, "\nEnding program (Interrupted SIGTERM)\n", 39);
    } else if (sig == 0) {
        write(1, "\nEnding program (User end)\n", 28);
    }

    // Ending program (via inetterupt)
    pthread_cancel(thr[READER]);
    pthread_cancel(thr[ANALYZER]);
    pthread_cancel(thr[PRINTER]);
    pthread_cancel(thr[LOGGER]);
    pthread_join(thr[READER], NULL);
    pthread_join(thr[ANALYZER], NULL);
    pthread_join(thr[PRINTER], NULL);
    pthread_join(thr[LOGGER], NULL);
    end[WATCHDOG] = 1;
}
