#ifndef II_EVENT_H
#define II_EVENT_H

static inline int iiGetArguments(const char *format, va_list vl, iiSingleArgument args_ret[][II_MAX_ARGUMENTS]) {
    int arg_count = strlen(format);
    int current_args_indx = 0;
    const char* name;
    int64_t i64;

    while ( current_args_indx < arg_count ) {
        name = va_arg(vl, const char*);

        iiArgType current_arg_type = (iiArgType) format[current_args_indx];
        iiSingleArgument *curr_arg = &(*args_ret)[current_args_indx];
        curr_arg->name = name;
        curr_arg->type = current_arg_type;
        switch ( current_arg_type ) {
            case CONST_STR:
                curr_arg->s = va_arg(vl, const char*);
                current_args_indx++;
                break;
            case INT:
                curr_arg->i = va_arg(vl, int32_t);
                LOG("III ARGS_case i current %d i %d", current_args_indx, curr_arg->i);
                current_args_indx++;
                break;
            case INT64:
                i64 = va_arg(vl, int64_t);
                curr_arg->i64 = i64;
                LOG("III ARGS_case i current %d i %ld", current_args_indx, curr_arg->i64);
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

void iiReleaseEvent(iiSingleEvent* e) {
    int expected = II_FILLING;
    int exchanged = atomic_compare_exchange_strong_explicit(&e->flushStatus, &expected, (int)II_READY_TO_FLUSH, II_memory_order_release, II_memory_order_relaxed);

    assert(exchanged);
    assert(expected == II_FILLING);
}

void iiPageAdded() {
    IIGlobalData *data = &__iiGlobalTracerData;
    pthread_cond_broadcast(&data->page_added);
}

static inline iiSingleEvent* iiEventFromNewPage() {
    IIGlobalData *data = &__iiGlobalTracerData;
    pthread_mutex_lock(&data->page_mutex);

    iiEventsPage* page = (iiEventsPage*) atomic_load(&data->tail);

    if (page) {
        int idx = atomic_fetch_add_explicit(&page->index, 1, II_memory_order_acquire);
        if ( idx <= eventsPerPage ) {
            pthread_mutex_unlock(&data->page_mutex);
            return &page->events[idx];
        }
    }

    iiEventsPage* newPage = iiFromFreeList();

    if (!newPage) {
        // we're definitely out of space. Let's allocate a new page from heap.
        newPage = (iiEventsPage*) calloc(sizeof(iiEventsPage), 1);
        atomic_store_explicit(&newPage->index, 1, II_memory_order_relaxed);
    }

    iiEventsPage* oldtail = (iiEventsPage*) atomic_load_explicit(&data->tail, II_memory_order_relaxed);
    atomic_store_explicit(&data->tail, (uintptr_t) newPage, II_memory_order_relaxed);

    iiEventsPage* i = data->flushQueue;
    if (i) {
        while (i->next) { i = i->next; }
        i->next = oldtail;
    } else {
        data->flushQueue = oldtail;
    }

    LOG("added page, %p, oldtail next %p", oldtail, oldtail ? oldtail->next : 0);

    iiPageAdded();
    pthread_mutex_unlock(&data->page_mutex);

    return &newPage->events[0];
}

static inline iiSingleEvent* iiGetNextEvent() {
    IIGlobalData *data = &__iiGlobalTracerData;

    iiEventsPage* page = (iiEventsPage*) atomic_load_explicit(&data->tail, II_memory_order_relaxed);

    if (!page)
        return iiEventFromNewPage();

    int idx = atomic_fetch_add_explicit(&page->index, 1, II_memory_order_acquire);
    if ( idx >= eventsPerPage )
        return iiEventFromNewPage();

    return &page->events[idx];
}

// NOTE: LIMITATION: name must be alive for the program lifetime
static inline void iiEvent(const char* name, iiEventType type) {
    iiSingleEvent *e = iiGetNextEvent();
    atomic_store_explicit(&e->flushStatus, (int)II_FILLING, II_memory_order_release);
    e->type = type;
    e->name = name;
    e->argCount = 0;
    e->tid = gettid();
    e->pid = getpid();
    e->time = iiCurrentTimeUs();

    iiReleaseEvent(e);
} // NOLINT "potential memory leak"

static inline int iiEventWithArgs(const char* name,
                                   iiEventType type,
                                   const char* format,
                                   va_list args) {
    iiSingleEvent *e = iiGetNextEvent();
    atomic_store_explicit(&e->flushStatus, (int)II_FILLING, II_memory_order_release);
    e->type = type;
    e->name = name;
    e->argCount = 0;
    e->tid = gettid();
    e->pid = getpid();
    e->time = iiCurrentTimeUs();

    e->argCount = iiGetArguments(format, args, &e->args);

    iiReleaseEvent(e);
    return e->argCount;
}
#endif
