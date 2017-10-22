
INITIAL_DIR := `pwd`

all: c cpp bench

bench:
	g++  -g -fno-omit-frame-pointer -O0 -std=c++11 -pthread gbenchmark/bench.cpp -I. -o bench -lbenchmark -fstack-protector-strong

cpp:
	g++  -g -std=c++11 -pthread tests/chrometracing.cpp -I. -o cpp
	./cpp

c:
	gcc  -g   -pthread tests/chrometracing.c -I. -o c
	II_FLUSH_INTERVAL=10 II_TRACE_FILE=/tmp/c.trace time ./c

clean:
	rm ./c ./cpp
