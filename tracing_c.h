#ifndef II_TRACING_C_H
#define II_TRACING_C_H

#include <stddef.h> // size_t
#include <pthread.h>

const char*    iiTraceFileEnvVar = "II_TRACE_FILE";
const char*    iiFlushIntervalEnvVar = "II_FLUSH_INTERVAL";
const char*    iiDefaultTraceFileName = "/tmp/out.trace";
const size_t   iiMaxArgumentsStrSize = 1024;

#include "tracing_c_internal.h"


__attribute__ ((weak)) void II_EVENT_START(const char* x) {
    pthread_once(&__iiGlobalTracerData.once_flag, iiInitGlobalData);
    double time = iiCurrentTimeUs();

    // FIXME: join fprintf + maybeFlush to a separate function
    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"B\", \"pid\": %d, \"tid\": %" PRId64 ", \"ts\": %f },\n", x, getpid(), gettid(), time);
    iiMaybeFlush();
}

__attribute__ ((weak)) void II_EVENT_END(const char* x) {
    double time = iiCurrentTimeUs();

    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"E\", \"pid\": %d, \"tid\": %" PRId64 ", \"ts\": %f },\n", x, getpid(), gettid(), time);
    iiMaybeFlush();
}

__attribute__ ((weak)) void II_EVENT_START_ARGS(const char* x, const char* format, ...) {
    pthread_once(&__iiGlobalTracerData.once_flag, iiInitGlobalData);
    va_list vl;
    char allargs[iiMaxArgumentsStrSize];

    va_start(vl, format);
    int converted = iiGetArgumentsJson(format, vl, allargs);
    va_end(vl);

    if (!converted)
        return;

    double time = iiCurrentTimeUs();

    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"B\", \"pid\": %d, \"tid\": %" PRId64 ", \"ts\": %f, \"args\": %s},\n", x, getpid(), gettid(), time, allargs);
    iiMaybeFlush();
}

#define II_TRACE_C_SCOPE(__name, ...)           \
    do {                                        \
        II_EVENT_START(__name);                 \
        __VA_ARGS__;                            \
        II_EVENT_END(__name);                   \
    } while(0)

#endif
