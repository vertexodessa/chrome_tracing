#ifndef II_UTILS_H
#define II_UTILS_H

static inline int iiDecrementWrapped(II_ATOMIC_INT* value, int boundary) {
    while (1) {
        int expected = 0;
        // we don't need to maintain memory consistency except the single "data->numEventsToFlush" variable,
        // so using _weak exchange should be safe here.
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

static inline int iiIncrementWrapped( II_ATOMIC_INT* i, int ceil ) {
    int expected, desired;
    do {
        expected = atomic_load(i);
        desired = expected + 1;
        if (desired >= ceil) desired = 0;
    } while (!atomic_compare_exchange_weak(i, &expected, desired));

    return desired;
}

static inline double iiCurrentTimeUs() {
    struct timespec  tm;
    clock_gettime(CLOCK_MONOTONIC, &tm);
    return (1000000000LL * tm.tv_sec + tm.tv_nsec) / 1000.0;
}
#endif
