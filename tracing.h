#ifndef TRACING_H
#define TRACING_H

#if !defined(__cplusplus)
#include "tracing_c.h"
#else
#include "tracing_cpp.h"
#endif

//// common API
// void __attribute__ ((constructor)) my_init(void);
// void __attribute__ ((destructor)) my_fini(void);

// Process global file handle: setenv() (synchronized with flock())
// Process global file handle: mkfifo with redirection to a file

// Process global variables: /dev/shm/iitrace.pid.lock + /dev/shm/iitrace.pid ??
// https://www.softprayog.in/programming/interprocess-communication-using-posix-shared-memory-in-linux
// Process global variables: header-only dummies with Weak symbol, loaded with Shared libs
// Process global variables: II_INITIALIZE_GLOBAL_VARIABLES macro

// Environment variables for customization:
// export II_FLUSH_INTERVAL=N
// Default: 1
// if set to an integer N -- file will be flushed each N events

// export II_TRACE_FILE="/tmp/out.trace"
// Default: /tmp/out.trace
// if set to a string -- this file is used for writing events in JSON format.

//// C API

// II_EVENT_START(name);
// II_EVENT_END(name);

// II_FUNC_START(name);
// II_FUNC_END(name);

// II_TRACE_C_SCOPE (name, {
//         funcCall();
//     });
//// is equivavent to ---->>
// II_EVENT_START(name);
// __VA_ARGS__;
// II_EVENT_END(name);

/* argTypes might come in the following format: "iIuUsc"
 * Where:
 * i: signed 32bit integer
 * I: signed 64bit integer
 * u: unsigned 32bit integer
 * U: unsigned 64bit integer
 * s: const char*
 * c: char
 */
// II_EVENT_START_ARGS(name, argTypes, ...)
// II_EVENT_END_ARGS(name, argTypes, ...);
// example: II_EVENT_START_ARGS("myFunc", "is", "integerArgName", intArg, "charpArgName", charpArg);

// II_FLUSH_EVENTS();

//// C++ API

// II_TRACE_CURRENT_SCOPE(name, &optional IIArguments arguments);
// II_TRACE_FUNCTION(&optional  IIArguments arguments);

// Redefines operators new and delete when added to a class
// II_TRACE_OBJECT_ALLOCATION

//// C++ internal stuff

// template <class T>
// IIStrArgument make_IIArgument(const std::string& name, T arg) {
//     return IIArgument(name, std::to_string(arg));
// }

// class IIStrArgument {
//     std::pair<std::string, std::string> arg;
// public:
//     IIStrArgument(const string& name, const string& val);
//     std::string asJson() const;
// };

// class IIArguments {
//     vector<IIStrArgument> args;
//     std::string asJson() const;
//};



// #define _POSIX_C_SOURCE 199309L
// #ifndef _GNU_SOURCE
//     #define _GNU_SOURCE
// #endif

// #include <assert.h>
// #include <dlfcn.h>
// #include <inttypes.h>
// #include <pthread.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <sys/syscall.h>
// #include <sys/time.h>
// #include <sys/types.h>
// #include <time.h>
// #include <unistd.h>

// #ifdef __cplusplus
//     #include <atomic>
//     using namespace std;
// #else
//     #include <stdatomic.h>
// #endif

// #ifdef SYS_gettid
//     #define gettid() syscall(SYS_gettid)
// #else
//     #error "SYS_gettid unavailable on this system"
// #endif

// #define ii_internal_buf_size 1024
// #define ii_internal_event_buf_size buf_size

// enum IIEventType {
//     DURATION_BEGIN   = 'B',
//     DURATION_END     = 'E',
//     COMPLETE         = 'X',
//     INSTANT          = 'i',
//     ASYNC_BEGIN      = 'b',
//     ASYNC_END        = 'e',
//     ASYNC_INSTANT    = 'n',
//     NEWOBJECT        = 'N',
//     DELETEOBJECT     = 'D',
//     COUNTER          = 'C',
//     FLOW_START       = 's',
//     FLOW_STEP        = 't',
//     FLOW_END         = 'f'
// };

// struct InstantEvent {
//     enum IIInstantEventScope {
//         GLOBAL   = 'g',
//         PROCESS  = 'p',
//         THREAD   = 't'
//     };
// };

// struct Args {
    
// };

// typedef struct IIEvent_ {
//     char text[ii_internal_buf_size + 1];
//     IIEventType type;
    
//     atomic_int flushed;
//     const char* id; // for async, object events
//     // DEBUG
//     int num;
//     int size;
// } IIEvent;


// global data
struct IIGlobalData {
    static IIGlobalDataImpl* get() {
        static IIGlobalDataImpl* data = IIInitData();
        return data;
    }
    static void init();

//    void lock();
//    void unlock();

    static int log(va_args args);
    static int log(const char* args, ...) /* printf format */;
};

#endif
