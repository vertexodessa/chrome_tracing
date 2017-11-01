# chrome_tracing
Simple C/C++ API to generate .trace files for chrome://tracing 

# some performance characteristics (actual on 2017-10-19)
```
Run on (8 X 3600 MHz CPU s)
2017-10-19 22:59:51
***WARNING*** CPU scaling is enabled, the benchmark real time measurements may be noisy and will incur extra overhead.
***WARNING*** Library was built as DEBUG. Timings may be affected.
--------------------------------------------------------------
Benchmark                       Time           CPU Iterations
--------------------------------------------------------------
BM_basic_output              3945 ns       3943 ns     177726
BM_int_argument_output       4333 ns       4332 ns     161639
BM_convert_time               325 ns        325 ns    2136579
```

# performance characteristics update (2017-11-01, threaded event flushing to a file):
```
Run on (8 X 3600 MHz CPU s)
2017-11-01 12:37:53
***WARNING*** Library was built as DEBUG. Timings may be affected.
--------------------------------------------------------------
Benchmark                       Time           CPU Iterations
--------------------------------------------------------------
BM_basic_output               507 ns        507 ns    1781755
BM_int_argument_output        661 ns        661 ns    1123887
BM_convert_time                24 ns         24 ns   28131823
BM_convert_args               459 ns        459 ns    1526272
```
Note: this is an "amortized" performance. Actual flush thread performance varies depending on the IO speed.

# Trace format description:
https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU/preview