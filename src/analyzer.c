#include "analyzer.h"

ssize_t get_line_len(char* buffer, ssize_t size) {
    ssize_t i = 0;
    while ((buffer[i] != '\n') && (buffer[i] != '\0') && (i < size))
        i++;
    if (i == size)
        return -1;
    i++;
    return i;
}

size_t get_cpu_count(char* stat_buffer, ssize_t size) {
    int rs = 0;
    ssize_t line_len = 0;
    size_t cpus_num = 0;
    char* buffer = stat_buffer;
    
    while (!rs) {
        // check if line starts with "cpu" string
        rs = strncmp(buffer, "cpu", 3);
        if (rs)
            break;
        cpus_num++;

        // get line length and change pointer
        line_len = get_line_len(buffer, size);

        if (line_len == -1)
            break;
        size -= line_len;
        buffer += line_len;
    }
    return cpus_num;
}

void analyze(struct cpu_time* cpu, size_t cpus_num, char* stat_buffer, ssize_t size) {

    unsigned long long int usertime, nicetime, systemtime, idletime;
    unsigned long long int ioWait, irq, softIrq, steal, guest, guestnice;

    int cpuid = 0;
    char* buffer = stat_buffer;
    ssize_t line_len;
    
    for (size_t i = 0; i < cpus_num; i++) {
        if (i == 0)
            (void) sscanf(buffer, "cpu  %20llu %20llu %20llu %20llu %20llu %20llu %20llu %20llu %20llu %20llu", &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
        else {
            (void) sscanf(buffer, "cpu%4d %20llu %20llu %20llu %20llu %20llu %20llu %20llu %20llu %20llu %20llu", &cpuid, &usertime, &nicetime, &systemtime, &idletime, &ioWait, &irq, &softIrq, &steal, &guest, &guestnice);
        }

        line_len = get_line_len(buffer, size);
        if (line_len == -1)
            break;
        size -= line_len;
        buffer += line_len;

        cpu[i].idle = idletime + ioWait;
        cpu[i].total = usertime + nicetime + systemtime + irq + softIrq + idletime + ioWait + steal + guest + guestnice;
    }
}
