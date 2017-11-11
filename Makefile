
INITIAL_DIR := `pwd`

INTERNAL_HEADERS = \
    internals/event.h \
    internals/flush_thread.h \
    internals/globals.h \
    internals/includes_defines.h \
    internals/initialisation.h \
    internals/tracing_c_internal.h \
    internals/types.h \
    internals/utils.h \

all: c cpp bench

bench: gbenchmark/bench.cpp tracing_c.h $(INTERNAL_HEADERS) tracing_cpp.h  tracing.h
	g++  -g -fno-omit-frame-pointer -O0 -std=c++11 -pthread gbenchmark/bench.cpp -I. -o bench -lbenchmark -fstack-protector-strong
	./bench

cpp: tests/chrometracing.cpp tracing_c.h $(INTERNAL_HEADERS) tracing_cpp.h  tracing.h
	g++  -g -std=c++11 -pthread tests/chrometracing.cpp -I. -o cpp
	./cpp

c: tests/chrometracing.c tracing_c.h $(INTERNAL_HEADERS)  tracing_cpp.h  tracing.h
	gcc  -g   -pthread tests/chrometracing.c -I. -o c
	II_FLUSH_INTERVAL=10 II_TRACE_FILE=/tmp/c.trace time ./c

clean:
	rm ./c ./cpp
