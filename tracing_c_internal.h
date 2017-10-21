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
    #define II_ATOMIC_BOOL std::atomic_bool
    #define II_memory_order_release std::memory_order_release
    #define II_memory_order_acquire std::memory_order_acquire
    #define II_memory_order_relaxed std::memory_order_relaxed
#else
    #include <stdatomic.h>
    #define II_ATOMIC_INT atomic_int
    #define II_ATOMIC_BOOL atomic_bool
    #define II_memory_order_release memory_order_release
    #define II_memory_order_acquire memory_order_acquire
    #define II_memory_order_relaxed memory_order_relaxed
#endif

typedef enum {
    II_FLUSHED         = 0,
    II_FILLING         = 1,
    II_READY_TO_FLUSH  = 1 << 1,
} IICellState;

typedef struct {
    iiArgType type;
    union {
        const char* s;
        int i;
        int64_t i64;
    };
} iiSingleArgument;

typedef struct {
    II_ATOMIC_INT flushStatus;
    const char* name;
    iiEventType type;
    size_t argCount;
    double time;
    pid_t pid;
    pid_t tid;
    // FIXME: current limitation is, that an event might have 5 arguments at max
    iiSingleArgument args[5];
} iiSingleEvent;

typedef struct __IIGlobalData {
    pthread_once_t once_flag;
    FILE* fd;
    int eventQueueSize;
#warning atomics only work with c11, need to use pthreads or GCC internals if c11 is not available
    II_ATOMIC_INT currentEventHint;
    II_ATOMIC_INT resourcesUsed;
    pthread_cond_t event_added;
    pthread_mutex_t event_added_mutex;
    pthread_t flush_thread;
    iiSingleEvent* queue;
} IIGlobalData;




__attribute__ ((weak)) IIGlobalData __iiGlobalTracerData;

static inline int iiEventQueueSizeFromEnv() {
    const char* envVarValue = getenv(iiEventQueueSize);
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

__attribute__ ((weak)) void* flush_thread( void* unused ) {
    (void) unused;

    IIGlobalData &data = __iiGlobalTracerData;
    while (true) {
        pthread_cond_wait(&data.event_added, &data.event_added_mutex);
        // wait until the queue is full enough
        if (atomic_load_explicit(&data.resourcesUsed, II_memory_order_acquire) < data.eventQueueSize / 2)
            continue;
        int i = atomic_load_explicit(&data.currentEventHint, II_memory_order_relaxed);
        while ( atomic_load_explicit(&data.queue[i].flushStatus, II_memory_order_acquire ) == II_READY_TO_FLUSH) {
            /* printf("flushed event %d, %s, used resources %d\n", i, data.queue[i].name, atomic_load(&data.resourcesUsed)); */
            iiSingleEvent& e = data.queue[i];
            fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"%c\", \"pid\": %d, \"tid\": %d, \"ts\": %f },\n", e.name, (char)e.type, e.pid, e.tid, e.time);

            atomic_store(&data.queue[i].flushStatus, (int) II_FLUSHED);
            atomic_fetch_sub_explicit(&data.resourcesUsed, 1, II_memory_order_release);
            ++i;
            i %= data.eventQueueSize;
        }
    }
    return NULL;
}

static inline void iiInitEnvironment() {
    IIGlobalData &data = __iiGlobalTracerData;
    data.fd = fopen(iiFileNameFromEnv(), "w");
    setvbuf(data.fd, NULL , _IOLBF , 4096);
    fprintf(data.fd, "[");
    data.eventQueueSize = iiEventQueueSizeFromEnv();
    data.queue = (iiSingleEvent*) calloc(sizeof(iiSingleEvent), data.eventQueueSize);
    data.currentEventHint = 0;

    pthread_condattr_t ignored;
    pthread_cond_init(&data.event_added, &ignored);

    pthread_mutexattr_t ignored1;
    pthread_mutex_init(&data.event_added_mutex, &ignored1);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&data.flush_thread, &attr, &flush_thread, NULL);
    data.resourcesUsed = 0;
}

static inline int iiDecrementWrapped(II_ATOMIC_INT* value, int boundary) {
    while (1) {
        int expected = 0;
        // we don't need to maintain memory consistency except the single "data->numEventsToFlush" variable,
        // so using _weak exchange should be saafe here.
        int swapped = atomic_compare_exchange_weak(value, &expected, boundary);
        if (swapped) {
            return 0;
            break;
        } else {
            int actual = expected;
            assert(actual > 0);
            swapped = atomic_compare_exchange_weak(value, &actual, actual - 1);
            if (swapped)
                return actual;
        }
    }
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

static inline int iiGetArguments(const char *format, va_list vl, iiSingleArgument (&args_ret)[5]) {
    int arg_count = strlen(format);
    char args[arg_count][iiMaxArgumentsStrSize];
    int current_args_indx = 0;
    const char* name;
    union {
        const char* s;
        int i;
        int64_t i64;
    };

    while ( current_args_indx < arg_count ) {
        name = va_arg(vl, const char*);

        iiArgType current_arg = (iiArgType) format[current_args_indx];

        args_ret[current_args_indx].type = current_arg;
        switch ( current_arg ) {
            case CONST_STR:
                s = va_arg(vl, const char*);
                // TODO: check for string overflow
                args_ret[current_args_indx].s = s;
                snprintf(args[current_args_indx], iiMaxArgumentsStrSize, "\"%s\": \"%s\"", name, s);
                current_args_indx++;
                break;
            case INT:
                i = va_arg(vl, int32_t);
                // fprintf(stderr, "III ARGS_case i current %d i %d\n" , current, i); fflush(stdout);
                args_ret[current_args_indx].i = i;
                snprintf(args[current_args_indx], iiMaxArgumentsStrSize, "\"%s\": %d", name, i);
                current_args_indx++;
                break;
            case INT64:
                i64 = va_arg(vl, int64_t);
                args_ret[current_args_indx].i64 = i64;
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

    return arg_count;
}

void iiEventAdded() {
    IIGlobalData &data = __iiGlobalTracerData;
    atomic_fetch_add_explicit(&data.resourcesUsed, 1, II_memory_order_release);
    pthread_cond_signal(&data.event_added);
}

static inline int iiIncrementWrapped( II_ATOMIC_INT* i, int ceil ) {
    int expected, desired;
    do {
        expected = atomic_load(i);
        desired = expected + 1;
        if (desired >= ceil) desired = 0;
    } while (!atomic_compare_exchange_weak(i, &expected, desired));

    return desired;
}

size_t iiGetNextEventIndex() {
    IIGlobalData &data = __iiGlobalTracerData;

    int eventIndex;
    int ignored = II_FLUSHED;
    do {
        eventIndex = iiIncrementWrapped(&data.currentEventHint, data.eventQueueSize);
        ignored = II_FLUSHED;
    } while( !atomic_compare_exchange_weak(&data.queue[eventIndex].flushStatus, &ignored, (int) II_FILLING) );

    return eventIndex;
}

void iiReleaseEvent(iiSingleEvent* e) {
    int expected = II_FILLING;
    int exchanged = atomic_compare_exchange_weak_explicit(&e->flushStatus, &expected, (int)II_READY_TO_FLUSH, II_memory_order_release, II_memory_order_relaxed);

    assert(exchanged);
    assert(expected == II_FILLING);

    iiEventAdded();
}

// FIXME: LIMITATION: name must be alive for the program lifetime
inline static void iiEvent(const char* name, iiEventType type) {
    IIGlobalData &data = __iiGlobalTracerData;

    size_t nextIdx = iiGetNextEventIndex();

    iiSingleEvent &e = data.queue[nextIdx];
    e.type = type;
    e.name = name;
    e.argCount = 0;
    e.tid = gettid();
    e.pid = getpid();
    e.time = iiCurrentTimeUs();

    iiReleaseEvent(&e);
}

inline static int iiEventWithArgs(const char* name,
                                   iiEventType type,
                                   const char* format,
                                   va_list args) {
    IIGlobalData &data = __iiGlobalTracerData;

    size_t nextIdx = iiGetNextEventIndex();

    iiSingleEvent &e = data.queue[nextIdx];
    e.type = type;
    e.name = name;
    e.argCount = 0;
    e.tid = gettid();
    e.pid = getpid();
    e.time = iiCurrentTimeUs();

    e.argCount = iiGetArguments(format, args, e.args);

    iiReleaseEvent(&e);
    return e.argCount;
}

#endif
