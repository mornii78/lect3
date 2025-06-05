#include "mysyslog.h"
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

static FILE *log_file = NULL;

void init_log(const char *filename) {
    log_file = fopen(filename, "a");
    if (!log_file) {
        fprintf(stderr, "Не удалось открыть лог-файл: %s\n", filename);
        log_file = stderr;
    }
}

void close_log(void) {
    if (log_file && log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
}

static void write_log(const char *level, const char *fmt, va_list args) {
    if (!log_file) log_file = stderr;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] [%s] ", time_buf, level);
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");

    fflush(log_file);
}

void log_info(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("INFO", fmt, args);
    va_end(args);
}

void log_warning(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("WARN", fmt, args);
    va_end(args);
}

void log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    write_log("ERROR", fmt, args);
    va_end(args);
}
