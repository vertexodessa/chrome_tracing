#ifndef II_FREELIST_H
#define II_FREELIST_H

static inline iiEventsPage* iiFromFreeList() {
    IIGlobalData *data = &__iiGlobalTracerData;
    iiEventsPage* ret = NULL;
    pthread_mutex_lock(&data->freeList_mutex);
    if (data->freeList) {
        ret = data->freeList;
        data->freeList = ret->next;
        ret->next = NULL;
    }
    pthread_mutex_unlock(&data->freeList_mutex);
    return ret;
}

static inline void iiAddToFreeList(iiEventsPage* page) {
    IIGlobalData *data = &__iiGlobalTracerData;

    memset(page, 0, sizeof(iiEventsPage));
    atomic_store_explicit(&page->index, 1, II_memory_order_release);

    pthread_mutex_lock(&data->freeList_mutex);
    if (!data->freeList) {
        data->freeList = page;
        pthread_mutex_unlock(&data->freeList_mutex);
        return;
    }
    iiEventsPage* last = data->freeList;
    while (last->next) { last = last->next; }
    last->next = page;

    pthread_mutex_unlock(&data->freeList_mutex);
}

#endif
