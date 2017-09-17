

all: c cpp

cpp:
	g++  -g -std=c++11 -pthread chrometracing.cpp -o cpp
	./cpp

c:
	gcc  -g   -pthread chrometracing.c -o c
	II_FLUSH_INTERVAL=10 time ./c

clean:
	rm ./c ./cpp
