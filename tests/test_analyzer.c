#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../src/analyzer.h"

int main() {
    int fd;
    ssize_t data_size;
    char data_buffer[4096];
    size_t cpus_num = 0;
    double total = 0;
    double idle = 0;
    double cpu_usage[31] = {0};
    struct cpu_time cpu_new_time[31] = {0};
    struct cpu_time cpu_old_time[31] = {0};

    printf("\nTEST Analyzer STARTED\n");

    // test on driver
    for (size_t j = 0; j < 20; j++) {
        fd = open("/proc/stat", O_RDONLY);
        assert(fd != -1);

        data_size = read(fd, data_buffer, 4000);
        assert(data_size != -1);
        assert(data_size !=  0);

        // check number of cpus
        cpus_num = get_cpu_count(data_buffer, data_size);
        assert(cpus_num > 0 && cpus_num <= 30);

        analyze(cpu_new_time, cpus_num, data_buffer, data_size);
        for (size_t i = 0; i < cpus_num; i++) {
            idle = (double)(cpu_new_time[i].idle - cpu_old_time[i].idle);
            total = (double)(cpu_new_time[i].total - cpu_old_time[i].total);
            cpu_usage[i] = ((total - idle) / total) * 100.0;
            // cpu usage should be in range <0.0; 100.0>
            assert(cpu_usage[i] >= 0.0 && cpu_usage[i] <= 100.0);
        }
        memcpy(cpu_old_time, cpu_new_time, sizeof(struct cpu_time) * cpus_num);
        close(fd);
        usleep(50000);
    }

    // test on custom file (30 cpu cores)
    fd = open("./tests/proc_stat_30_cpu_cores.txt", O_RDONLY);
    assert(fd != -1);

    data_size = read(fd, data_buffer, 4000);
    assert(data_size != -1);
    assert(data_size !=  0);

    // check number of cpus
    cpus_num = get_cpu_count(data_buffer, data_size);
    assert(cpus_num == 31); // first record is sum of all cpu cores
    
    close(fd);
    usleep(50000);

    printf("TEST Analyzer PASSED\n");

    return 0;
}
