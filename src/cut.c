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


static pthread_t thr_reader;
static pthread_t thr_analyzer;
static pthread_t thr_printer;
static pthread_t thr_logger;

static pthread_mutex_t mtx_reader = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_printer = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mtx_logger = PTHREAD_MUTEX_INITIALIZER;

static pthread_cond_t cond_var_reader = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_var_printer = PTHREAD_COND_INITIALIZER;

static struct queue q_reader;
static struct queue q_printer;
static struct queue q_logger;

static int end_program;

static int fd;
static FILE* log_stream;

static struct logger_t log_queue = {.q = &q_logger,
                                    .mtx = &mtx_logger};

// watchdog
int main() {

    int c = 0;
    struct termios old_settings = {0};
    struct termios new_settings = {0};

    // status of thread:
    //    1) thread set status to 1
    //    2) watchdog resetes status to 0 with 2 sec intervals
    //    3) if status 0 after 2 sec - thread hanged
    size_t status_reader = 0;
    size_t status_analyzer = 0;
    size_t status_printer = 0;
    size_t status_logger = 0;

    struct sigaction sa = {0};
    sa.sa_handler = exit_handler;

    // queue init
    init_queue(&q_reader, 100, 3000);
    init_queue(&q_printer, 100, 3000);
    init_queue(&q_logger, 100, 3000);

    // changing terminal mode (input available immediately)
    tcgetattr(STDIN_FILENO, &old_settings);
    memcpy(&new_settings, &old_settings, sizeof(struct termios));
    new_settings.c_lflag -= ICANON;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_settings);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);

    // creating log file stream
    log_stream = fopen("log.txt", "a");
    log_direct_message(log_stream, "[WATCHDOG] - Starting program");

    // creating threads
    pthread_create(&thr_reader, NULL, reader, &status_reader);
    pthread_create(&thr_analyzer, NULL, analyzer, &status_analyzer);
    pthread_create(&thr_printer, NULL, printer, &status_printer);
    pthread_create(&thr_logger, NULL, logger, &status_logger);

    // seting signals handler
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    // watchdog code; cyclic 2 sec check
    for (size_t i = 1; !end_program && !(c == 'q'); i++) {
        c = getc(stdin);
        usleep(100000);
        i %= 20;
        if (!i) {
            if (!status_reader) {
                log_direct_message(log_stream, "[WATCHDOG] - Reader hanging. Ending...");
                break;
            }
            if (!status_analyzer) {
                log_direct_message(log_stream, "[WATCHDOG] - Analyzer hanging. Ending...");
                break;
            }
            if (!status_printer) {
                log_direct_message(log_stream, "[WATCHDOG] - Printer hanging. Ending...");
                break;
            }
            if (!status_logger) {
                log_direct_message(log_stream, "[WATCHDOG] - Logger hanging. Ending...");
                break;
            }
            log_message(&log_queue, "[WATCHDOG] - CHECK all thread OK - "
                                    "queues: q[reader] = %zu/%zu | q[analyzer] = %zu/%zu",
                                     q_reader.size, q_reader.max_queue_size, 
                                     q_printer.size, q_printer.max_queue_size);
            status_reader = 0;
            status_analyzer = 0;
            status_printer = 0;
            status_logger = 0;
        }
    }
    // changing terminal mode back
    tcsetattr(STDIN_FILENO, TCSANOW, &old_settings);

    // Ending program
    if (!end_program)
        exit_handler(0);

    log_direct_message(log_stream, "[WATCHDOG] - END - closing files and freeing memory");

    // Cleanup
    close(fd);
    fclose(log_stream);
    deinit_queue(&q_reader);
    deinit_queue(&q_printer);
    deinit_queue(&q_logger);

    return 0;
}





void* reader(void* arg) {
    size_t* status = (size_t*)arg;

    int rs = 1;
    int rs_mtx = 1;
    ssize_t data_size;
    char data_buffer[4096] = {0};

    size_t delay = 0;

    for(;;) {
        *status = 1;
        delay++;
        delay %= 10;
        if (delay) {
            // FIXME only for testing
            usleep(100000);
            continue;
        }

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

        rs_mtx = pthread_mutex_lock(&mtx_reader);
        if (rs_mtx) {
            log_message(&log_queue, "[READER] - Could not unlock: READERS mtx");
            *status = 0;
            pthread_exit(status);
        }
        // enqueue data for analyzer
        rs = enqueue(&q_reader, data_buffer, (size_t)data_size);
        pthread_cond_signal(&cond_var_reader);
        rs_mtx = pthread_mutex_unlock(&mtx_reader);
        if (rs_mtx) {
            log_message(&log_queue, "[READER] - Could not unlock: READERS mtx");
            *status = 0;
            pthread_exit(status);
        }

        if (!rs)
            log_message(&log_queue, "[READER] - Read %zu bytes and added to Queue (queue[READER].size = %zu)", data_size, q_reader.size);

        close(fd);
    }
}





static void* analyzer(void* arg) {
    size_t* status = (size_t*)arg;

    struct timespec ts;
    struct timeval  tv;
    long timeout = 500l; // thread condition timeout in miliseconds

    int rs = 1;
    int rs_mtx = 1;
    size_t data_size = 0;
    char data_buffer_raw[4096] = {0};

    // I assume that there will not be more then 30 cores
    size_t cpus_num = 0;
    double total = 0;
    double idle = 0;
    double cpu_usage[30] = {0};
    struct cpu_time cpu_new_time[30] = {0};
    struct cpu_time cpu_old_time[30] = {0};

    for(;;) {
        *status = 1;

        rs_mtx = pthread_mutex_lock(&mtx_reader);
        if (rs_mtx) {
            log_message(&log_queue, "[ANALYZER] - Could not acquire lock: READERS mtx");
            *status = 0;
            pthread_exit(status);
        }
        // Wait for signal but with 1 sec timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_nsec = (tv.tv_usec * 1000l) + timeout * 1000000; // 500 [ms]
        ts.tv_sec = tv.tv_sec + (ts.tv_nsec / 1000000000l);
        ts.tv_nsec %= 1000000000l;
        pthread_cond_timedwait(&cond_var_reader, &mtx_reader, &ts);
        // dequeue data from reader
        rs = dequeue(&q_reader, data_buffer_raw, &data_size);
        rs_mtx = pthread_mutex_unlock(&mtx_reader);
        if (rs_mtx) {
            log_message(&log_queue, "[ANALYZER] - Could not unlock: READERS mtx");
            *status = 0;
            pthread_exit(status);
        }

        // analyze data
        if (!rs) {
            log_message(&log_queue, "[ANALYZER] - Dequeued %zu bytes from Readers Queue", data_size);
        } else {
            continue;
        }

        // get number of cpus (cpus num is always too big by one)
        cpus_num = get_cpu_count(data_buffer_raw, (ssize_t)data_size);
        if (cpus_num < 2) {
            log_message(&log_queue, "[ANALYZER] - Cpus number = %zu;less than 2", cpus_num);
            *status = 0;
            pthread_exit(status);
        }
        // get new cpu idle/total time data and compare it with previous one 
        // after that determine cpu_usage
        analyze(cpu_new_time, cpus_num ,data_buffer_raw, (ssize_t)data_size);
        for (size_t i = 0; i < cpus_num; i++) {
            idle = (double)(cpu_new_time[i].idle - cpu_old_time[i].idle);
            total = (double)(cpu_new_time[i].total - cpu_old_time[i].total);
            cpu_usage[i] = ((total - idle) / total) * 100.0;
        }
        memcpy(cpu_old_time, cpu_new_time, sizeof(struct cpu_time) * cpus_num);
        log_message(&log_queue, "[ANALYZER] - Data analyzed, defined cpu cores: %zu", cpus_num);

        rs_mtx = pthread_mutex_lock(&mtx_printer);
        if (rs_mtx) {
            log_message(&log_queue, "[ANALYZER] - Could not acquire lock: PRINTERS mtx");
            *status = 0;
            pthread_exit(status);
        }
        // enqueue data for printer
        rs = enqueue(&q_printer, cpu_usage, (size_t)(cpus_num * sizeof(double)));
        pthread_cond_signal(&cond_var_printer);
        rs_mtx = pthread_mutex_unlock(&mtx_printer);
        if (rs_mtx) {
            log_message(&log_queue, "[ANALYZER] - Could not unlock: PRINTERS mtx");
            *status = 0;
            pthread_exit(status);
        }

        if (!rs)
            log_message(&log_queue, "[ANALYZER] - Added data to Printers Queue (queue[ANALYZER].size = %zu)", q_printer.size);
    }
}





static void* printer(void* arg) {
    size_t* status = (size_t*)arg;

    struct timespec ts;
    struct timeval  tv;
    long timeout = 500l; // thread condition timeout in miliseconds

    int rs = 1;
    int rs_mtx = 1;
    size_t data_size = 0;
    size_t cpus_num = 0;
    double cpu_usage[30] = {0};

    int first_time = 1;

    for(;;) {
        *status = 1;

        rs_mtx = pthread_mutex_lock(&mtx_printer);
        if (rs_mtx) {
            log_message(&log_queue, "[PRINTER] - Could not acquire lock: PRINTERS mtx");
            *status = 0;
            pthread_exit(status);
        }
        
        // Wait for signal but with 500 ms timeout (just in case if reader hanged or ended)
        gettimeofday(&tv, NULL);
        ts.tv_nsec = (tv.tv_usec * 1000l) + timeout * 1000000; // 500 [ms]
        ts.tv_sec = tv.tv_sec + (ts.tv_nsec / 1000000000l);
        ts.tv_nsec %= 1000000000l;
        pthread_cond_timedwait(&cond_var_printer, &mtx_printer, &ts);
        // dequeue data from analyzer
        rs = dequeue(&q_printer, cpu_usage, &data_size);
        rs_mtx = pthread_mutex_unlock(&mtx_printer);
        if (rs_mtx) {
            log_message(&log_queue, "[PRINTER] - Could not unlock: PRINTERS mtx");
            *status = 0;
            pthread_exit(status);
        }

        if (!rs) {
            log_message(&log_queue, "[PRINTER] - Dequeued %zu bytes from printers Queue", data_size);
        } else {
            continue;
        }

        // print data to stdin
        cpus_num = ((size_t)data_size / sizeof(double));
        for (size_t i = 0; i < cpus_num; i++) {
            if (!i) {
                if (!first_time)
                    printf("\033[%zuA", cpus_num + 1);
                printf("\033[0K\tcpu overal usage: \033[1m%3.2f %%\033[22m\n", cpu_usage[i]);
            } else {
                printf("\033[0K\tcpu%2zu: \033[1m%3.2f %%\033[22m\n", i, cpu_usage[i]);
            }
        }
        printf("Press 'q' to exit\n");
        first_time = 0;
    }
}





static void* logger(void* arg) {
    size_t* status = (size_t*)arg;

    int rs_mtx = 1;

    char message[300] = {0};
    size_t data_size;

    for(;;) {
        *status = 1;

        rs_mtx = pthread_mutex_lock(&mtx_logger);
        if (rs_mtx) {
            *status = 0;
            pthread_exit(status);
        }
        while (q_logger.size > 0) {
            dequeue(&q_logger, message, &data_size);
            fprintf(log_stream, "%s", message);
        }
        rs_mtx = pthread_mutex_unlock(&mtx_logger);
        if (rs_mtx) {
            *status = 0;
            pthread_exit(status);
        }
        usleep(1000000);
    }
}





static void exit_handler(int sig) {

    // Ending threads
    pthread_cancel(thr_reader);
    pthread_cancel(thr_analyzer);
    pthread_cancel(thr_printer);
    pthread_cancel(thr_logger);
    pthread_join(thr_reader, NULL);
    pthread_join(thr_analyzer, NULL);
    pthread_join(thr_printer, NULL);
    pthread_join(thr_logger, NULL);

    if (sig == SIGINT) {
        write(1, "\nEnding program (Interrupted SIGINT)\n", 38);
    } else if (sig == SIGTERM) {
        write(1, "\nEnding program (Interrupted SIGTERM)\n", 39);
    } else if (sig == 0) {
        write(1, "\nEnding program (User end)\n", 28);
    }

    end_program = 1;
}
