#ifndef MYSYSLOG_H
#define MYSYSLOG_H

#include <stdio.h>

void init_log(const char *filename);
void close_log(void);

void log_info(const char *fmt, ...);
void log_warning(const char *fmt, ...);
void log_error(const char *fmt, ...);

#endif
