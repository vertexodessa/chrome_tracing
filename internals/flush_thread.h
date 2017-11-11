#ifndef II_FLUSH_THREAD_H
#define II_FLUSH_THREAD_H

void iiStop() {
    IIGlobalData *data = &__iiGlobalTracerData;
    atomic_store(&data->running, 0);
    pthread_cond_broadcast(&data->page_added);
}

void iiJoinThread() {
    IIGlobalData *data = &__iiGlobalTracerData;
    pthread_join(data->flush_thread, NULL);
}

static inline int iiJoinArguments(size_t arg_count, iiSingleArgument args[][II_MAX_ARGUMENTS],  char out[iiMaxArgumentsStrSize]) {
    int pos = 0;
    int current_args_indx = 0;

    while (current_args_indx < arg_count) {
        iiSingleArgument* curr = &(*args)[current_args_indx];
        switch (curr->type) {
            case CONST_STR:
                pos += sprintf(out+pos, "\"%s\": \"%s\"", curr->name, curr->s);
                break;
            case INT:
                pos += sprintf(out+pos, "\"%s\": %d", curr->name, curr->i);
                break;
            case INT64:
                pos += sprintf(out+pos, "\"%s\": %" PRIu64, curr->name, curr->i64);
                break;
            default:
                assert(0);
                return 0;
        }
        out[pos++] = ',';
        ++current_args_indx;
    }

    out[pos-1] = '\0';
    pos = pos > 0 ? pos-1 : 0;

    assert(pos < iiMaxArgumentsStrSize);
    return pos;
}

static inline void iiFlushEvent(iiSingleEvent* e) {
    if (e->argCount == 0)
        fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"%c\", \"pid\": %d, \"tid\": %d, \"ts\": %f },\n", e->name, (char)e->type, e->pid, e->tid, e->time);
    else
    {
        char args[iiMaxArgumentsStrSize];
        iiJoinArguments(e->argCount, &e->args, args);
        fprintf(__iiGlobalTracerData.fd, "{\"name\": \"%s\", \"cat\": \"PERF\", \"ph\": \"%c\", \"pid\": %d, \"tid\": %d, \"ts\": %f, \"args\": {%s} },\n", e->name, (char)e->type, e->pid, e->tid, e->time, args);
    }

    atomic_store(&e->flushStatus, (int) II_FLUSHED);

    LOG("flushed event %s", e->name);
}

static inline void iiFlushPage(iiEventsPage* p) {
    for( int i = 0; i < eventsPerPage; ++i ) {
        while (atomic_load_explicit(&p->events[i].flushStatus, II_memory_order_acquire) != II_READY_TO_FLUSH) { }
        iiFlushEvent(&p->events[i]);
    }
}

static inline void iiFlushAllPages() {
    IIGlobalData *data = &__iiGlobalTracerData;
    iiEventsPage *nextPage = data->flushQueue;

    int i=0;
    while (nextPage) {
        ++i;
        LOG("flushing page %d", i);
        iiFlushPage(nextPage);
        iiEventsPage *tmp = nextPage;
        nextPage = tmp->next;
        free(tmp);
    }

    data->flushQueue = NULL;
}

__attribute__ ((weak)) void* flush_thread( void* unused ) {
    (void) unused;

    IIGlobalData *data = &__iiGlobalTracerData;
    pthread_mutex_lock(&data->page_mutex);
    while (atomic_load(&data->running)) {
        int err;
        while ((err = pthread_cond_wait(&data->page_added, &data->page_mutex)) != 0) {  }
        iiEventsPage *tmp;
      flushNext:
        tmp = data->flushQueue;
        if (!tmp)
            continue;
        data->flushQueue = tmp->next;
        pthread_mutex_unlock(&data->page_mutex);
        iiFlushPage(tmp);
        free(tmp);
        pthread_mutex_lock(&data->page_mutex);
        goto flushNext;
    }

    iiFlushAllPages();
    pthread_mutex_unlock(&data->page_mutex);
    fflush(data->fd);
    LOG("!!!Flushed the data");

    return NULL;
}

#endif
