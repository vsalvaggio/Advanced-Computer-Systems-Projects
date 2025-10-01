#include <immintrin.h>   // AVX2 intrinsics
#include <chrono>
#include <iostream>
#include <random>
#include <vector>
#include <cstring>
#include <cstdlib>

// Compile with:
//   g++ -O3 -march=native -std=c++17 -o saxpy saxpy.cpp
// Enable SIMD explicitly with -DSIMD

// Simple timing helper
double now_seconds() {
    using clock = std::chrono::high_resolution_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
    if (argc < 3 || std::strcmp(argv[1], "--size") != 0) {
        std::cerr << "Usage: ./saxpy --size N\n";
        return 1;
    }

    size_t N = std::stoull(argv[2]);
    float a = 2.5f;

    std::vector<float> x(N), y(N);

    // Initialize with random values
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < N; i++) {
        x[i] = dist(rng);
        y[i] = dist(rng);
    }

    // Warm-up to avoid cold start effects
    for (volatile int i = 0; i < 1000; i++) {
        y[i % N] += a * x[i % N];
    }

    double t0 = now_seconds();

#ifdef SIMD
    // SIMD AVX2 version: process 8 floats per iteration
    size_t i = 0;
    __m256 alpha = _mm256_set1_ps(a);
    for (; i + 7 < N; i += 8) {
        __m256 xv = _mm256_loadu_ps(&x[i]);
        __m256 yv = _mm256_loadu_ps(&y[i]);
        yv = _mm256_fmadd_ps(alpha, xv, yv); // yv = a*xv + yv
        _mm256_storeu_ps(&y[i], yv);
    }
    // Remainder
    for (; i < N; i++) {
        y[i] += a * x[i];
    }
#else
    // Scalar fallback
    for (size_t i = 0; i < N; i++) {
        y[i] += a * x[i];
    }
#endif

    double t1 = now_seconds();

    // Compute FLOPs and bandwidth
    double elapsed = t1 - t0;
    double flops = 2.0 * N; // one multiply + one add per element
    double gflops = flops / (elapsed * 1e9);

    double bytes = 2.0 * N * sizeof(float); // read x + read/write y
    double bandwidth = bytes / (elapsed * 1e9); // GB/s

    std::cout << "N=" << N
              << " time=" << elapsed << " s"
              << " GFLOP/s=" << gflops
              << " BW=" << bandwidth << " GB/s"
#ifdef SIMD
              << " [SIMD]"
#else
              << " [Scalar]"
#endif
              << std::endl;

    return 0;
}