#ifndef II_INITIALISATION_H
#define II_INITIALISATION_H

__attribute__((destructor)) void dtor() {
    LOG("EXITING!!!!!");
    iiStop();
    iiJoinThread();
}

static inline const char* iiFileNameFromEnv() {
    const char* ret = getenv(iiTraceFileEnvVar);
    return ret ?: iiDefaultTraceFileName;
}

static inline int iiEventQueueSizeFromEnv() {
    const char* envVarValue = getenv(iiEventQueueSize);
    int ret = 0;
    if (envVarValue) {
        ret = strtol (envVarValue, NULL, 10);
    }

    return ret ?: 100;
}


static inline void iiInitEnvironment() {
    IIGlobalData *data = &__iiGlobalTracerData;
    memset(data, 0, sizeof(IIGlobalData));

    data->fd = fopen(iiFileNameFromEnv(), "w");
    setvbuf(data->fd, NULL , _IOLBF , 4096);
    fprintf(data->fd, "[");
    data->eventQueueSize = iiEventQueueSizeFromEnv();
    atomic_store(&data->running, 1);

    pthread_condattr_t ignored;
    if (pthread_cond_init(&data->page_added, &ignored))
        printf("ERROR: can't init conditional variable\n");

    pthread_mutex_init(&data->freeList_mutex, NULL);
    pthread_mutex_init(&data->page_mutex, NULL);

    pthread_attr_t attr;
    pthread_attr_init(&attr);

    pthread_create(&data->flush_thread, &attr, &flush_thread, NULL);
}
#endif
