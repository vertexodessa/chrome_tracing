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
    #define II_ATOMIC_PTR std::atomic_uintptr_t
    #define II_memory_order_release std::memory_order_release
    #define II_memory_order_acquire std::memory_order_acquire
    #define II_memory_order_relaxed std::memory_order_relaxed
#else
    #include <stdatomic.h>
    #define II_ATOMIC_INT atomic_int
    #define II_ATOMIC_BOOL atomic_bool
    #define II_ATOMIC_PTR atomic_uintptr_t
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

const int eventsPerPage = 1024;
typedef struct __iiEventsPage {
    struct __iiEventsPage* next;
    II_ATOMIC_INT index;

// FIXME: don't hardcode event count    
    iiSingleEvent events[eventsPerPage];
} iiEventsPage;

typedef struct __IIGlobalData {
    II_ATOMIC_INT running;
    int thread_finished;

    pthread_once_t once_flag;
    FILE* fd;

    pthread_cond_t page_added;
    iiEventsPage* flushQueue;
    pthread_t flush_thread;

    II_ATOMIC_PTR tail;
    pthread_mutex_t page_mutex;

    int eventQueueSize;
} IIGlobalData;




__attribute__ ((weak)) IIGlobalData __iiGlobalTracerData;

static inline int iiEventQueueSizeFromEnv() {
    const char* envVarValue = getenv(iiEventQueueSize);
    int ret = 0;
    if (envVarValue) {
        ret = strtol (envVarValue, NULL, 10);
    }

    return ret ?: 100;
}

static inline const char* iiFileNameFromEnv() {
    const char* ret = getenv(iiTraceFileEnvVar);
    return ret ?: iiDefaultTraceFileName;
}

void iiStop() {
    IIGlobalData &data = __iiGlobalTracerData;
    atomic_store(&data.running, 0);
    /* while(!data.thread_finished)  */
    pthread_cond_broadcast(&data.page_added);
}

void iiJoinThread() {
    IIGlobalData &data = __iiGlobalTracerData;
    pthread_join(data.flush_thread, NULL);
}

__attribute__((destructor)) void dtor() {
    printf("EXITING!!!!!\n"); fflush(stdout);
    iiStop();
    iiJoinThread();
}

static inline void iiFlushEvent(iiSingleEvent& e) {
    fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"%c\", \"pid\": %d, \"tid\": %d, \"ts\": %f },\n", e.name, (char)e.type, e.pid, e.tid, e.time);

    atomic_store(&e.flushStatus, (int) II_FLUSHED);

    /* IIGlobalData &data = __iiGlobalTracerData; */
    /* printf("flushed event %d, %s, used resources %d\n", i, e.name, atomic_load(&data.resourcesUsed)); */
}

static inline void iiFlushPage(iiEventsPage& p) {
    for( int i = 0; i < eventsPerPage; ++i ) {
        while (atomic_load_explicit(&p.events[i].flushStatus, II_memory_order_acquire) != II_READY_TO_FLUSH) { }
        iiFlushEvent(p.events[i]);
    }
}

static inline void iiFlushAllPages() {
    IIGlobalData &data = __iiGlobalTracerData;
    iiEventsPage *nextPage = data.flushQueue;

    int i=0;
    while (nextPage) {
        ++i;
        /* printf("flushing page %d\n", i); */
        iiFlushPage(*nextPage);
        iiEventsPage *tmp = nextPage;
        nextPage = tmp->next;
        free(tmp);
    }

    data.flushQueue = NULL;
}

__attribute__ ((weak)) void* flush_thread( void* unused ) {
    (void) unused;

    IIGlobalData &data = __iiGlobalTracerData;
    pthread_mutex_lock(&data.page_mutex);
    while (atomic_load(&data.running)) {
        int err;
        while ((err = pthread_cond_wait(&data.page_added, &data.page_mutex)) != 0) {  }
      flushNext:
        iiEventsPage *tmp = data.flushQueue;
        data.flushQueue = tmp->next;
        pthread_mutex_unlock(&data.page_mutex);
        iiFlushPage(*tmp);
        free(tmp);
        pthread_mutex_lock(&data.page_mutex);
        if (data.flushQueue)
            goto flushNext;
        //iiFlushAllPages();
    }

    iiFlushAllPages();
    pthread_mutex_unlock(&data.page_mutex);
    fflush(__iiGlobalTracerData.fd);
    /* printf("!!!Flushed the data\n"); */
    data.thread_finished = 1;

    return NULL;
}

static inline void iiInitEnvironment() {
    IIGlobalData &data = __iiGlobalTracerData;
    data.fd = fopen(iiFileNameFromEnv(), "w");
    setvbuf(data.fd, NULL , _IOLBF , 4096);
    fprintf(data.fd, "[");
    data.eventQueueSize = iiEventQueueSizeFromEnv();
    atomic_store(&data.running, 1);

    data.thread_finished = 0;

    pthread_condattr_t ignored;
    if (pthread_cond_init(&data.page_added, &ignored))
        printf("ERROR: can't init cond var\n");

    data.page_mutex = PTHREAD_MUTEX_INITIALIZER;

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&data.flush_thread, &attr, &flush_thread, NULL);
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


static inline int iiIncrementWrapped( II_ATOMIC_INT* i, int ceil ) {
    int expected, desired;
    do {
        expected = atomic_load(i);
        desired = expected + 1;
        if (desired >= ceil) desired = 0;
    } while (!atomic_compare_exchange_weak(i, &expected, desired));

    return desired;
}

void iiReleaseEvent(iiSingleEvent* e) {
    int expected = II_FILLING;
    int exchanged = atomic_compare_exchange_strong_explicit(&e->flushStatus, &expected, (int)II_READY_TO_FLUSH, II_memory_order_release, II_memory_order_relaxed);

    assert(exchanged);
    assert(expected == II_FILLING);
}

void iiPageAdded() {
    IIGlobalData &data = __iiGlobalTracerData;
    pthread_cond_broadcast(&data.page_added);
}

static inline iiSingleEvent& iiEventFromNewPage() {
    IIGlobalData &data = __iiGlobalTracerData;
    pthread_mutex_lock(&data.page_mutex);

    iiEventsPage* page = (iiEventsPage*) atomic_load(&data.tail);

    if (page) {
// relax mem order    
        int idx = atomic_fetch_add(&page->index, 1);
        if ( idx <= eventsPerPage ) {
            pthread_mutex_unlock(&data.page_mutex);
            return page->events[idx];
        }
    }

    // we're definitely out of space. Let's allocate a new page.
    iiEventsPage* newPage = (iiEventsPage*) calloc(sizeof(iiEventsPage), 1);
    iiEventsPage* oldtail = (iiEventsPage*) atomic_load(&data.tail);
    atomic_store(&newPage->index, 1);
    atomic_store(&data.tail, (uintptr_t) newPage);

    iiEventsPage* i = data.flushQueue;
    if (i) {
        while (i->next)
            i = i->next;
        i->next = oldtail;
    } else {
        data.flushQueue = oldtail;
    }

    /* printf("added page, %p, oldtail next %p\n", oldtail, oldtail ? oldtail->next : 0); */

// relax mem order    
    iiPageAdded();
    pthread_mutex_unlock(&data.page_mutex);
    //pthread_yield();

    return newPage->events[0];
}

static inline iiSingleEvent& iiGetNextEvent() {
    IIGlobalData &data = __iiGlobalTracerData;

// relax mem order     
    iiEventsPage* page = (iiEventsPage*) atomic_load(&data.tail);

    if (!page)
        return iiEventFromNewPage();

// relax mem order    
    int idx = atomic_fetch_add(&page->index, 1);
    if ( idx >= eventsPerPage )
        return iiEventFromNewPage();

    return page->events[idx];
}

// FIXME: LIMITATION: name must be alive for the program lifetime
inline static void iiEvent(const char* name, iiEventType type) {
    iiSingleEvent &e = iiGetNextEvent();
    atomic_store_explicit(&e.flushStatus, (int)II_FILLING, II_memory_order_release);
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
    return 1;
    IIGlobalData &data = __iiGlobalTracerData;

    iiSingleEvent &e = iiGetNextEvent();
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
