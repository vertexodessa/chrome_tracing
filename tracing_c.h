#ifndef II_TRACING_C_H
#define II_TRACING_C_H

#define _POSIX_C_SOURCE 199309L
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef SYS_gettid
#define gettid() syscall(SYS_gettid)
#else
#error "SYS_gettid unavailable on this system"
#endif

typedef struct __IIGlobalData {
    FILE* fd;
    int flushInterval;
#warning atomic only works with c11, need to use pthreads if c11 is not available
    _Atomic int numEventsToFlush;
} IIGlobalData;

static inline int iiFlushIntervalFromEnv() {
    const char* envVarValue = getenv("II_FLUSH_INTERVAL");
    int ret = 0;
    if (envVarValue) {
        ret = strtol (envVarValue, NULL, 10);
    }

    return ret ?: 1;
};

static inline const char* iiFileNameFromEnv() {
    const char* ret = getenv("II_TRACE_FILE");
    return ret ?: "/tmp/out.trace";
};

#define II_DECLARE_GLOBAL_VARIABLES() IIGlobalData __iiGlobalTracerData;
#define II_INIT_GLOBAL_VARIABLES()                                      \
    do {                                                                \
	__iiGlobalTracerData.fd = fopen(iiFileNameFromEnv(), "w");      \
	fprintf(__iiGlobalTracerData.fd, "[");                          \
        __iiGlobalTracerData.flushInterval = iiFlushIntervalFromEnv();  \
        __iiGlobalTracerData.numEventsToFlush = __iiGlobalTracerData.flushInterval; \
    } while (0)

extern IIGlobalData __iiGlobalTracerData;

static inline void iiMaybeFlush(IIGlobalData* data) {
    while (1) {
        int expected = 0;
        _Bool swapped = atomic_compare_exchange_strong(&data->numEventsToFlush, &expected, data->flushInterval);
        if (swapped) {
            fflush(data->fd);
            break;
        } else {
            int actual = expected;
            assert(actual > 0);
            swapped = atomic_compare_exchange_strong(&data->numEventsToFlush, &actual, actual - 1);
            if(swapped)
                break;
        }
    }
}

static inline void II_EVENT_START(const char* x) {
    struct timespec  tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    uint64_t tm64 = 1000000000LL * tm.tv_sec + tm.tv_nsec;
    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"B\", \"pid\": %d, \"tid\": %" PRId64 ", \"ts\": %f, \"args\": {\"dummy\" : %" PRId64 "} },\n", x, getpid(), gettid(), tm64/1000.0, tm64);
    iiMaybeFlush(&__iiGlobalTracerData);
}

static inline void II_EVENT_END(const char* x) {
    struct timespec  tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    uint64_t tm64 = 1000000000LL * tm.tv_sec + tm.tv_nsec;
    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"E\", \"pid\": %d, \"tid\": %" PRId64 ", \"ts\": %f },\n", x, getpid(), gettid(), tm64/1000.0);
    iiMaybeFlush(&__iiGlobalTracerData);
}

// example: II_EVENT_START_ARGS("myFunc", "is", "integerArgName", intArg, "charpArgName", charpArg)

static inline void II_EVENT_START_ARGS(const char* x, const char* format, ...) {
    const int MAX_ARG_SIZE = 1024;
    int arg_count = strlen(format);
    int current_args_indx = 0;
    va_list vl;
    va_start(vl, format);

    char args[arg_count][MAX_ARG_SIZE];
    const char* s;
    const char* name;
    int i;
    int64_t i64;
    while ( current_args_indx < arg_count ) {
        name = va_arg(vl, const char*);
        switch (format[current_args_indx]) {
            case 's':
                s = va_arg(vl, const char*);
                // TODO: check for string overflow
                snprintf(args[current_args_indx], MAX_ARG_SIZE, "\"%s\": \"%s\"", name, s);
                current_args_indx++;
                break;
            case 'i':
                i = va_arg(vl, int32_t);
                // fprintf(stderr, "III ARGS_case i current %d i %d\n" , current, i); fflush(stdout);
                snprintf(args[current_args_indx], MAX_ARG_SIZE, "\"%s\": %d", name, i);
                current_args_indx++;
                break;
            case 'l':
                i64 = va_arg(vl, int64_t);
                // fprintf(stderr, "III ARGS_case i current %d i %d\n" , current, i); fflush(stdout);
                snprintf(args[current_args_indx], MAX_ARG_SIZE, "\"%s\": %" PRId64, name, i64);
                current_args_indx++;
                break;
            default:
                assert(0);
                // unknown argument
                break;
        }
    }
    va_end(vl);

    char allargs[MAX_ARG_SIZE];
    int pos = 1;
    allargs[0] = '{';
    current_args_indx = 0;
    int len = strlen(args[current_args_indx]);

    const size_t COMMA = 1;
    const size_t BRACKET = 1;
    const size_t NULLSYMBOL = 1;
    const size_t MAX_SYMBOL_POS = MAX_ARG_SIZE - NULLSYMBOL - BRACKET;

    while ( pos + len < MAX_SYMBOL_POS ) {
        strcpy(allargs + pos, args[current_args_indx]);

        // terminate current argument in allargs
        allargs[pos + len] =
            (current_args_indx < arg_count - 1) ? ',' : '\0';

        pos += len + COMMA;

        if (++current_args_indx >= arg_count)
            break;
        len = strlen(args[current_args_indx]);
    }

    allargs[pos-1] = '}';
    allargs[pos]   = '\0';

    struct timespec  tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    uint64_t tm64 = 1000000000LL * tm.tv_sec + tm.tv_nsec;
    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"B\", \"pid\": %d, \"tid\": %" PRId64 ", \"ts\": %f, \"args\": %s},\n", x, getpid(), gettid(), tm64/1000.0, allargs);
    iiMaybeFlush(&__iiGlobalTracerData);
}

#define II_TRACE_C_SCOPE(__name, ...)           \
    II_EVENT_START(__name);                     \
    __VA_ARGS__                                 \
    II_EVENT_END(__name);                       \

#endif
