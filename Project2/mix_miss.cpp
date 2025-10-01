// mix_miss.cpp
#include <bits/stdc++.h>
#include <random>
#include <chrono>
using namespace std;
using ns = chrono::duration<double>;

int main(int argc, char **argv){
    if(argc < 5){
        fprintf(stderr, "usage: %s <hot_bytes> <cold_bytes> <stride> <hot_frac(0-1)>\n", argv[0]);
        return 1;
    }
    size_t hot_bytes = stoull(argv[1]);
    size_t cold_bytes = stoull(argv[2]);
    size_t stride = stoull(argv[3]);
    double hot_frac = stod(argv[4]);

    size_t hot_n = max((size_t)1, hot_bytes / stride);
    size_t cold_n = max((size_t)1, cold_bytes / stride);

    vector<char> hot(hot_bytes);
    vector<char> cold(cold_bytes);

    // Simple indices
    vector<size_t> hot_idx(hot_n), cold_idx(cold_n);
    iota(hot_idx.begin(), hot_idx.end(), 0);
    iota(cold_idx.begin(), cold_idx.end(), 0);

    mt19937_64 rng(12345);
    uniform_real_distribution<double> prob(0.0,1.0);
    uniform_int_distribution<size_t> hotpick(0, hot_n-1);
    uniform_int_distribution<size_t> coldpick(0, cold_n-1);

    volatile uint64_t sink = 0;
    const size_t iters = 5ULL<<22; // tune for runtime
    auto t0 = chrono::high_resolution_clock::now();
    for(size_t i=0;i<iters;i++){
        double r = prob(rng);
        if(r < hot_frac){
            // access hot
            size_t idx = hotpick(rng) * stride;
            sink += hot[idx];
            hot[idx] ^= 1;
        } else {
            size_t idx = coldpick(rng) * stride;
            sink += cold[idx];
            cold[idx] ^= 1;
        }
    }
    auto t1 = chrono::high_resolution_clock::now();
    double sec = chrono::duration_cast<ns>(t1-t0).count();
    printf("iters=%zu sec=%.6f ops/s=%.3f sink=%lu\n", iters, sec, iters/sec, (unsigned long)sink);
    return 0;
}