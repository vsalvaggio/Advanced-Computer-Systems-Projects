CXX=g++
CXXFLAGS=-O3 -march=native -std=c++17 -pthread

all: benchmark
benchmark: benchmark.cpp filters.cpp hash.h
	$(CXX) $(CXXFLAGS) benchmark.cpp -o benchmark

clean:
	rm -f benchmark
