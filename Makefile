CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I.
LDLIBS ?= -lz

build/benchmark: tests/benchmark.cpp $(wildcard compress/*.h)
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ tests/benchmark.cpp $(LDLIBS)

run: build/benchmark
	./build/benchmark data/biodata.csv

clean:
	rm -rf build

.PHONY: run clean
