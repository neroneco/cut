#ifndef _ANALYZER_H
#define _ANALYZER_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

struct cpu_time {
    unsigned long long int idle;
    unsigned long long int total;
};

ssize_t get_line_len(char* buffer, ssize_t size);

size_t get_cpu_count(char* stat_buffer, ssize_t size);

void analyze(struct cpu_time* cpu, size_t cpus_num, char* stat_buffer, ssize_t size);

#endif
