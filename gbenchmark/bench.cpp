#include "tracing.h"

#include <benchmark/benchmark.h>

II_DECLARE_GLOBAL_VARIABLES();

static void BM_basic_output(benchmark::State& state) {
    II_INIT_GLOBAL_VARIABLES();
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

BENCHMARK_MAIN();
