#include "tracing.h"

#include <benchmark/benchmark.h>

static void BM_basic_output(benchmark::State& state) {
    for (auto _ : state) {
        II_EVENT_START(__PRETTY_FUNCTION__);
        II_EVENT_END(__PRETTY_FUNCTION__);
    }
}
// Register the function as a benchmark
BENCHMARK(BM_basic_output);

static void BM_int_argument_output(benchmark::State& state) {
    for (auto _ : state) {
        II_EVENT_START_ARGS(__PRETTY_FUNCTION__, "i", "test_arg", 93);
        II_EVENT_END(__PRETTY_FUNCTION__);
    }
}
// Register the function as a benchmark
BENCHMARK(BM_int_argument_output);

static void BM_convert_time_helper(const char* x, const char* format, ...) {
    iiSingleArgument allargs[II_MAX_ARGUMENTS];
    int converted;

    va_list vl;
    va_start(vl, format);
    converted = iiGetArguments(format, vl, &allargs);
    va_end(vl);

    if (!converted)
        return;
}

static void BM_convert_time(benchmark::State& state) {
    for (auto _ : state) {
        BM_convert_time_helper("test", "is", "testname", 64, "test1", "teststring1");
    }
}

// Register the function as a benchmark
BENCHMARK(BM_convert_time);

static void BM_convert_args(benchmark::State& state) {
    char out[1024];

    iiSingleArgument allargs[II_MAX_ARGUMENTS];

    allargs[0].type = CONST_STR;
    allargs[0].name = "str_arg_name";
    allargs[0].s = "string argument";

    allargs[1].type = INT64;
    allargs[1].name = "i64_arg_name";
    allargs[1].i64 = 10000000LL;

    allargs[2].type = INT;
    allargs[2].name = "int_arg_name";
    allargs[2].i = 65537;

    for (auto _ : state) {
        iiJoinArguments(3, &allargs, out);
    }
    const char* ref_str = "{\"str_arg_name\": \"string argument\",\"i64_arg_name\": 10000000,\"int_arg_name\": 65537}";
    assert(strcmp(out, ref_str) == 0);
}

// Register the function as a benchmark
BENCHMARK(BM_convert_args);

BENCHMARK_MAIN();
