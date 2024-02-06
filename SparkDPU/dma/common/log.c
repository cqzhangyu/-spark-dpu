#include "log.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static int log_level = LOG_LEVEL;
static int log_level_mask = LOG_LEVEL_MASK;

static FILE *f = NULL;

int log_open(const char *fname) {
    log_close(f);
    f = fopen(fname, "w");
    if (f == 0) {
        printf("Cannot open log_file %s\n", fname);
        exit(1);
    }
    return 0;
}

inline int set_log_level(int level) {
    log_level = level;
}

inline int get_log_level() {
    return log_level;
}

int set_log_level_str(const char *str) {
    if (strcmp(str, "INFO") == 0)
        log_level = LOG_LEVEL_INFO;
    else if (strcmp(str, "ERRO") == 0)
        log_level = LOG_LEVEL_ERRO;
    else if (strcmp(str, "WARN") == 0)
        log_level = LOG_LEVEL_WARN;
    else if (strcmp(str, "PERF") == 0)
        log_level = LOG_LEVEL_PERF;
    else if (strcmp(str, "DEBUG") == 0)
        log_level = LOG_LEVEL_DEBUG;
    else
        printf("Unknown log level %s\n", str);
}

int set_log_level_mask(int mask) {
    log_level_mask = mask;
}

int lprintf(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    fflush(stdout);

    if (f) {
        va_start(ap, fmt);
        vfprintf(f, fmt, ap);
        va_end(ap);

        fflush(f);
    }
}

int log_print(int level, const char *fmt, ...) {
    if (level <= log_level && ((log_level_mask >> level) & 1)) {
        va_list ap;
        
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
        fflush(stdout);
        
        if (f) {
            va_start(ap, fmt);
            vfprintf(f, fmt, ap);
            va_end(ap);

            fflush(f);
        }
    }
}

void log_close() {
    if (f) {
        fclose(f);
        f = NULL;
    }
}
