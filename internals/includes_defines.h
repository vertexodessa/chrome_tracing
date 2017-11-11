#ifndef II_INCLUDES_DEFINES_H
#define II_INCLUDES_DEFINES_H

#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 199309L
#endif
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

#if 1
    #define LOG(...)
#else
    #define LOG(...)  do { printf("LOG: "); printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while(0)
#endif

#endif
