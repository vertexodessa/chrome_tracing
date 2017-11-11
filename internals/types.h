#ifndef II_TYPES_H
#define II_TYPES_H

typedef enum {
    II_FLUSHED         = 0,
    II_FILLING         = 1,
    II_READY_TO_FLUSH  = 1 << 1,
} IICellState;

typedef struct {
    const char* name;
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
    iiSingleArgument args[II_MAX_ARGUMENTS];
} iiSingleEvent;

#define eventsPerPage 4096
typedef struct __iiEventsPage {
    struct __iiEventsPage* next;
    II_ATOMIC_INT index;

// FIXME: don't hardcode event count    
    iiSingleEvent events[eventsPerPage];
} iiEventsPage;

typedef struct __IIGlobalData {
    II_ATOMIC_INT running;

    pthread_once_t once_flag;
    FILE* fd;

    pthread_cond_t page_added;
    iiEventsPage* flushQueue;
    pthread_t flush_thread;

    II_ATOMIC_PTR tail;
    pthread_mutex_t page_mutex;

    iiEventsPage* freeList;
    pthread_mutex_t freeList_mutex;

    int eventQueueSize;
} IIGlobalData;

#endif
