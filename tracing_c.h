#ifndef II_TRACING_C_H
#define II_TRACING_C_H

#include <stddef.h> // size_t
#include <pthread.h>

const char*    iiTraceFileEnvVar = "II_TRACE_FILE";
const char*    iiFlushIntervalEnvVar = "II_FLUSH_INTERVAL";
const char*    iiDefaultTraceFileName = "/tmp/out.trace";
const size_t   iiMaxArgumentsStrSize = 1024;

typedef enum {
    INT = 'i',
    INT64 = 'l',
    CONST_STR = 's'
} iiArgType;

typedef enum {
    EVENT_START = 'B',
    EVENT_END = 'E'
} iiEventType;

#include "tracing_c_internal.h"

// TODO: improve the timing twice by using COMPLETE events ('X') instead

__attribute__ ((weak)) void II_EVENT_START(const char* name) {
    pthread_once(&__iiGlobalTracerData.once_flag, iiInitEnvironment);

    const iiEventType eventType = EVENT_START;
    iiEvent(name, eventType);
}

__attribute__ ((weak)) void II_EVENT_END(const char* name) {
    const iiEventType eventType = EVENT_END;
    iiEvent(name, eventType);
}

// example: II_EVENT_START_ARGS("myFunc", "is", "integerArgName", 42, "charpArgName", "arg");
__attribute__ ((weak)) void II_EVENT_START_ARGS(const char* name, const char* format, ...) {
    pthread_once(&__iiGlobalTracerData.once_flag, iiInitEnvironment);
    va_list vl;
    char allargs[iiMaxArgumentsStrSize];

    va_start(vl, format);
    int converted = iiGetArgumentsJson(format, vl, allargs);
    va_end(vl);

    if (!converted)
        return;

    const iiEventType eventType = EVENT_START;
    iiEventWithArgs(name, eventType, allargs);
}

#define II_TRACE_C_SCOPE(__name, ...)           \
    do {                                        \
        II_EVENT_START(__name);                 \
        __VA_ARGS__;                            \
        II_EVENT_END(__name);                   \
    } while(0)

#endif
