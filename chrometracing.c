#include "tracing_c.h"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

II_DECLARE_GLOBAL_VARIABLES();

void f3() {
    II_TRACE_C_SCOPE ("sleep call (scope trace in f3)",
    {
        struct timespec to_sleep;
        to_sleep.tv_sec = 0;
        to_sleep.tv_nsec = 1 * 1000;
        nanosleep(&to_sleep, NULL);
    });
}

void f2(int arg) {
    II_EVENT_START_ARGS(__PRETTY_FUNCTION__, "is", "arg", arg, "test", "str");
    struct timespec to_sleep;
    to_sleep.tv_sec = 0;
    to_sleep.tv_nsec = 1 * 1000;
    nanosleep(&to_sleep, NULL);
    II_EVENT_END(__PRETTY_FUNCTION__);
}

void f1() {
    II_EVENT_START(__PRETTY_FUNCTION__);
    struct timespec to_sleep;
    to_sleep.tv_sec = 0;
    to_sleep.tv_nsec = 1 * 1000;
    nanosleep(&to_sleep, NULL);

    f2(23);
    II_EVENT_END(__PRETTY_FUNCTION__);
}

int main() {
    II_INIT_GLOBAL_VARIABLES();
    f1();
    II_TRACE_C_SCOPE ("f3_scope_from_main", {
            f3();
        });
}
