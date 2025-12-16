// hash.h
#pragma once
#include <stdint.h>
#include <immintrin.h>

inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

inline uint64_t hash64(uint64_t key, uint64_t seed = 0) {
    return mix64(key + 0x9e3779b97f4a7c15ULL + seed * 0xbf58476d1ce4e5b9ULL);
}
