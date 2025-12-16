// filters.cpp
// ECSE 4320 Project A3 — Approximate Membership Filters
// Implements Blocked Bloom, XOR, Cuckoo, and Quotient Filters
// Author: Vito Salvaggio

#include <bits/stdc++.h>
#include "hash.h"
using namespace std;

// ================================================================
// Common Interface
// ================================================================
class Filter {
public:
    virtual void insert(uint64_t key) = 0;
    virtual bool query(uint64_t key) = 0;
    virtual void remove(uint64_t key) {}
    virtual size_t size_bytes() const = 0;
    virtual ~Filter() = default;
};

// ================================================================
// 1. Blocked Bloom Filter
// ================================================================
class BloomFilter : public Filter {
    vector<uint64_t> bits;
    size_t nbits, k, nblocks;
public:
    BloomFilter(size_t n_entries, double target_fpr) {
        double m = -1.44 * n_entries * log2(target_fpr);
        nbits = (size_t)m;
        bits.resize((nbits + 63) / 64);
        k = max(1, int(round((nbits / (double)n_entries) * log(2))));
        nblocks = bits.size();
    }

    void insert(uint64_t key) override {
        for (size_t i = 0; i < k; i++) {
            uint64_t h = hash64(key, i);
            bits[(h % nbits) / 64] |= (1ULL << (h % 64));
        }
    }

    bool query(uint64_t key) override {
        for (size_t i = 0; i < k; i++) {
            uint64_t h = hash64(key, i);
            if (!(bits[(h % nbits) / 64] & (1ULL << (h % 64)))) return false;
        }
        return true;
    }

    size_t size_bytes() const override { return bits.size() * 8; }
};

// ================================================================
// 2. XOR Filter (Static Build)
// Simplified for demonstration; uses 3 hashes and prefilled keys.
// ================================================================
class XORFilter : public Filter {
    vector<uint8_t> fp;
    size_t size;
public:
    XORFilter(size_t n_entries, int fp_bits = 8) {
        size = n_entries * 1.23; // 23% overhead typical
        fp.resize(size);
    }

    void build(const vector<uint64_t>& keys) {
        for (auto k : keys) {
            uint64_t h1 = hash64(k, 0) % size;
            uint64_t h2 = hash64(k, 1) % size;
            uint64_t h3 = hash64(k, 2) % size;
            fp[h1] = fp[h2] ^ fp[h3] ^ (uint8_t)(hash64(k, 3) & 0xFF);
        }
    }

    bool query(uint64_t key) override {
        uint64_t h1 = hash64(key, 0) % size;
        uint64_t h2 = hash64(key, 1) % size;
        uint64_t h3 = hash64(key, 2) % size;
        uint8_t res = fp[h1] ^ fp[h2] ^ fp[h3];
        return res == (uint8_t)(hash64(key, 3) & 0xFF);
    }

    void insert(uint64_t) override {}
    size_t size_bytes() const override { return fp.size(); }
};

// ================================================================
// 3. Cuckoo Filter (Dynamic, safer version)
// ================================================================
class CuckooFilter : public Filter {
    static const int BUCKET_SIZE = 4;
    static const int MAX_KICKS = 500;

    struct Bucket {
        uint8_t fp[BUCKET_SIZE];
        Bucket() { memset(fp, 0, BUCKET_SIZE); }
    };

    vector<Bucket> table;
    size_t n_buckets;

public:
    CuckooFilter(size_t n_entries) {
        // allocate more space to avoid overfill crashes
        n_buckets = (size_t)(n_entries / BUCKET_SIZE * 2.0);
        table.resize(n_buckets);
    }

    inline uint8_t fingerprint(uint64_t key) const {
        uint64_t h = hash64(key, 0);
        return (uint8_t)((h & 0xFF) + 1); // never zero
    }

    inline size_t index1(uint64_t key) const {
        return hash64(key, 1) % n_buckets;
    }

    inline size_t index2(size_t i1, uint8_t fp) const {
        // XOR the fingerprint with a hash of itself, then mod table size
        return (i1 ^ (hash64(fp, 2) % n_buckets)) % n_buckets;
    }

    void insert(uint64_t key) override {
        uint8_t fp = fingerprint(key);
        size_t i1 = index1(key);
        size_t i2 = index2(i1, fp);

        // try both buckets directly
        for (auto i : {i1, i2}) {
            for (int j = 0; j < BUCKET_SIZE; j++) {
                if (table[i].fp[j] == 0) {
                    table[i].fp[j] = fp;
                    return;
                }
            }
        }

        // perform eviction if both full
        size_t i = (rand() & 1) ? i1 : i2;
        for (int kick = 0; kick < MAX_KICKS; kick++) {
            int j = rand() % BUCKET_SIZE;
            uint8_t old = table[i].fp[j];
            table[i].fp[j] = fp;
            fp = old;
            i = index2(i, fp);
            if (i >= n_buckets) i %= n_buckets; // safety wrap
            for (int k = 0; k < BUCKET_SIZE; k++) {
                if (table[i].fp[k] == 0) {
                    table[i].fp[k] = fp;
                    return;
                }
            }
        }

        // if we reach here, insertion failed (table too full)
        // (In production, you’d grow or stash.)
    }

    bool query(uint64_t key) override {
        uint8_t fp = fingerprint(key);
        size_t i1 = index1(key);
        size_t i2 = index2(i1, fp);
        for (auto i : {i1, i2})
            for (int j = 0; j < BUCKET_SIZE; j++)
                if (table[i].fp[j] == fp) return true;
        return false;
    }

    void remove(uint64_t key) override {
        uint8_t fp = fingerprint(key);
        size_t i1 = index1(key);
        size_t i2 = index2(i1, fp);
        for (auto i : {i1, i2})
            for (int j = 0; j < BUCKET_SIZE; j++)
                if (table[i].fp[j] == fp) {
                    table[i].fp[j] = 0;
                    return;
                }
    }

    size_t size_bytes() const override {
        return n_buckets * sizeof(Bucket);
    }
};


// ================================================================
// 4. Quotient Filter (Optimized Dynamic)
// ================================================================
#include <immintrin.h> // AVX2 intrinsics

class QuotientFilter : public Filter {
    struct alignas(64) Slot { // align to cache line
        uint16_t remainder;
        uint8_t meta; // bit0=occupied, bit1=shifted
    };

    vector<Slot> table;
    size_t qbits, rbits, size;
    const uint8_t OCCUPIED = 1;
    const uint8_t SHIFTED  = 2;
    const size_t MAX_SHIFT = 32;  // limit cluster shifts
    const size_t SIMD_WIDTH = 16; // AVX2: 16x16-bit integers

public:
    QuotientFilter(size_t n_entries, int rbits_ = 8) {
        qbits = ceil(log2(n_entries));
        rbits = rbits_;
        size = (1ULL << qbits) * 2; // low load factor for faster inserts
        table.resize(size);
        memset(table.data(), 0, table.size() * sizeof(Slot));
    }

    void insert(uint64_t key) override {
        uint64_t h = hash64(key, 0);
        size_t q = (h >> rbits) % size;
        uint16_t r = h & ((1ULL << rbits) - 1);

        size_t pos = q;
        size_t shift_count = 0;
        while (table[pos].meta & OCCUPIED) {
            pos = (pos + 1) % size;
            shift_count++;
            if (shift_count > MAX_SHIFT) return; // bounded shift
        }

        table[pos].remainder = r;
        table[pos].meta = OCCUPIED | ((pos != q) ? SHIFTED : 0);
    }

    bool query(uint64_t key) override {
        uint64_t h = hash64(key, 0);
        size_t q = (h >> rbits) % size;
        uint16_t r = h & ((1ULL << rbits) - 1);

        size_t pos = q;
        size_t scanned = 0;
        __m256i target_fp = _mm256_set1_epi16(r);

        while ((table[pos].meta & OCCUPIED) && scanned < MAX_SHIFT) {
            size_t remaining = min(SIMD_WIDTH, size - pos);
            __m256i data = _mm256_loadu_si256((__m256i*)&table[pos]);
            __m256i cmp = _mm256_cmpeq_epi16(data, target_fp);
            int mask = _mm256_movemask_epi8(cmp);
            if (mask) return true;

            pos = (pos + SIMD_WIDTH) % size;
            scanned += SIMD_WIDTH;
        }
        return false;
    }

    void remove(uint64_t key) override {
        uint64_t h = hash64(key, 0);
        uint16_t r = h & ((1ULL << rbits) - 1);
        for (auto &s : table) {
            if ((s.meta & OCCUPIED) && s.remainder == r) {
                s.meta = 0;
                return;
            }
        }
    }

    size_t size_bytes() const override {
        return table.size() * sizeof(Slot);
    }
};


// ================================================================
// Export creation functions
// ================================================================
Filter* make_filter(const string& type, size_t n_entries, double fpr = 0.01) {
    if (type == "bloom") return new BloomFilter(n_entries, fpr);
    if (type == "xor")   return new XORFilter(n_entries);
    if (type == "cuckoo")return new CuckooFilter(n_entries);
    if (type == "quotient")return new QuotientFilter(n_entries);
    return nullptr;
}
