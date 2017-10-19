#ifndef II_TRACING_C_INTERNAL
#define II_TRACING_C_INTERNAL

#define _POSIX_C_SOURCE 199309L
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE
#endif

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
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

// atomic libraries are different for C/C++ modes
#ifdef __cplusplus
    #include <atomic>
    #define II_ATOMIC_INT std::atomic_int
#else
    #include <stdatomic.h>
    #define II_ATOMIC_INT atomic_int
#endif

typedef struct __IIGlobalData {
    pthread_once_t once_flag;
    FILE* fd;
    int flushInterval;
#warning atomic only works with c11, need to use pthreads if c11 is not available
    II_ATOMIC_INT numEventsToFlush;
} IIGlobalData;

IIGlobalData __iiGlobalTracerData __attribute__ ((weak));

static inline int iiFlushIntervalFromEnv() {
    const char* envVarValue = getenv(iiFlushIntervalEnvVar);
    int ret = 0;
    if (envVarValue) {
        ret = strtol (envVarValue, NULL, 10);
    }

    return ret ?: 1;
}

static inline const char* iiFileNameFromEnv() {
    const char* ret = getenv(iiTraceFileEnvVar);
    return ret ?: iiDefaultTraceFileName;
}

static inline void iiInitGlobalData() {
    __iiGlobalTracerData.fd = fopen(iiFileNameFromEnv(), "w");
    setvbuf(__iiGlobalTracerData.fd, NULL , _IOLBF , 4096);
    fprintf(__iiGlobalTracerData.fd, "[");
    __iiGlobalTracerData.flushInterval = iiFlushIntervalFromEnv();
    __iiGlobalTracerData.numEventsToFlush = __iiGlobalTracerData.flushInterval;
}

static inline int iiDecrementAndReturnNextIndex() {
    IIGlobalData *data = &__iiGlobalTracerData;
    while (1) {
        int expected = 0;
        // we don't need to maintain memory consistency except the single "data->numEventsToFlush" variable,
        // so using _weak exchange should be saafe here.
        int swapped = atomic_compare_exchange_weak(&data->numEventsToFlush, &expected, data->flushInterval);
        if (swapped) {
            return 0;
            break;
        } else {
            int actual = expected;
            assert(actual > 0);
            swapped = atomic_compare_exchange_weak(&data->numEventsToFlush, &actual, actual - 1);
            if(swapped)
                return actual;
        }
    }
}

static inline void iiMaybeFlush() {
    if (iiDecrementAndReturnNextIndex() == 0)
        fflush(__iiGlobalTracerData.fd);
}

static inline double iiCurrentTimeUs() {
    struct timespec  tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    return (1000000000LL * tm.tv_sec + tm.tv_nsec) / 1000.0;
}

static inline int iiJoinArguments(size_t arg_count, const char (*args)[iiMaxArgumentsStrSize],  char* out) {
    int pos = 1;
    out[0] = '{';
    int current_args_indx = 0;
    int len = strlen(args[current_args_indx]);

    const size_t COMMA = 1;
    const size_t BRACKET = 1;
    const size_t NULLSYMBOL = 1;
    const size_t MAX_SYMBOL_POS = iiMaxArgumentsStrSize - NULLSYMBOL - BRACKET;

    while ( pos + len < MAX_SYMBOL_POS ) {
        strcpy(out + pos, args[current_args_indx]);

        // terminate current argument in out
        out[pos + len] =
            (current_args_indx < arg_count - 1) ? ',' : '\0';

        pos += len + COMMA;

        if (++current_args_indx >= arg_count)
            break;
        len = strlen(args[current_args_indx]);
    }

    out[pos-1] = '}';
    out[pos]   = '\0';

    return 1;
}

static inline int iiGetArgumentsJson(const char *format, va_list vl, char *out) {
    int arg_count = strlen(format);
    char args[arg_count][iiMaxArgumentsStrSize];
    int current_args_indx = 0;
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
                snprintf(args[current_args_indx], iiMaxArgumentsStrSize, "\"%s\": \"%s\"", name, s);
                current_args_indx++;
                break;
            case 'i':
                i = va_arg(vl, int32_t);
                // fprintf(stderr, "III ARGS_case i current %d i %d\n" , current, i); fflush(stdout);
                snprintf(args[current_args_indx], iiMaxArgumentsStrSize, "\"%s\": %d", name, i);
                current_args_indx++;
                break;
            case 'l':
                i64 = va_arg(vl, int64_t);
                // fprintf(stderr, "III ARGS_case i current %d i %d\n" , current, i); fflush(stdout);
                snprintf(args[current_args_indx], iiMaxArgumentsStrSize, "\"%s\": %" PRId64, name, i64);
                current_args_indx++;
                break;
            default:
                assert(0);
                return 0;
                // unknown argument
                break;
        }
    }

    iiJoinArguments(arg_count, args, out);
    return 1;
}

#endif
