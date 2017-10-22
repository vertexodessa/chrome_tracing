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



BENCHMARK_MAIN();
