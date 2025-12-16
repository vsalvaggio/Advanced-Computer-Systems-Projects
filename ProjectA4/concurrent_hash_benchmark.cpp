// concurrent_hash_benchmark_csv.cpp
// ECSE 4320 Project A4 — Hash Table Benchmark with CSV Output
// Author: Vito Salvaggio

#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <mutex>
#include <thread>
#include <random>
#include <chrono>
#include <atomic>
#include <algorithm>

using Key = int;
using Value = int;

// ===================== Hash Tables =====================

// 1. Coarse-Grained Lock
class CoarseHashTable {
private:
    std::vector<std::list<std::pair<Key, Value>>> table;
    size_t n_buckets;
    std::mutex global_mutex;

public:
    CoarseHashTable(size_t buckets) : n_buckets(buckets), table(buckets) {}

    void insert(Key key, Value value) {
        std::lock_guard<std::mutex> lock(global_mutex);
        size_t idx = key % n_buckets;
        table[idx].emplace_back(key, value);
    }

    bool find(Key key, Value &value) {
        std::lock_guard<std::mutex> lock(global_mutex);
        size_t idx = key % n_buckets;
        for (auto &kv : table[idx]) {
            if (kv.first == key) {
                value = kv.second;
                return true;
            }
        }
        return false;
    }

    void erase(Key key) {
        std::lock_guard<std::mutex> lock(global_mutex);
        size_t idx = key % n_buckets;
        table[idx].remove_if([key](auto &kv){ return kv.first == key; });
    }
};

// 2. Fine-Grained Lock (One mutex per bucket)
class FineHashTable {
private:
    std::vector<std::list<std::pair<Key, Value>>> table;
    std::vector<std::mutex> bucket_mutexes;
    size_t n_buckets;

public:
    FineHashTable(size_t buckets) : n_buckets(buckets), table(buckets), bucket_mutexes(buckets) {}

    void insert(Key key, Value value) {
        size_t idx = key % n_buckets;
        std::lock_guard<std::mutex> lock(bucket_mutexes[idx]);
        table[idx].emplace_back(key, value);
    }

    bool find(Key key, Value &value) {
        size_t idx = key % n_buckets;
        std::lock_guard<std::mutex> lock(bucket_mutexes[idx]);
        for (auto &kv : table[idx]) {
            if (kv.first == key) {
                value = kv.second;
                return true;
            }
        }
        return false;
    }

    void erase(Key key) {
        size_t idx = key % n_buckets;
        std::lock_guard<std::mutex> lock(bucket_mutexes[idx]);
        table[idx].remove_if([key](auto &kv){ return kv.first == key; });
    }
};

// ===================== Benchmark Utilities =====================
enum class WorkloadType { LookupOnly, InsertOnly, Mixed7030 };

template<typename Table>
void run_workload(Table &table, WorkloadType workload,
                  const std::vector<Key> &keys,
                  size_t start_idx, size_t end_idx,
                  std::atomic<size_t> &ops_done) {
    std::random_device rd;
    std::mt19937 gen(rd());

    size_t total = end_idx - start_idx;
    size_t mixed_writes = 0;
    if (workload == WorkloadType::Mixed7030)
        mixed_writes = total * 0.3;

    for (size_t i = start_idx; i < end_idx; i++) {
        if (workload == WorkloadType::InsertOnly) {
            table.insert(keys[i], keys[i]*2);
        } else if (workload == WorkloadType::LookupOnly) {
            Value val;
            table.find(keys[i], val);
        } else { // Mixed 70/30
            if (i - start_idx < mixed_writes) {
                table.insert(keys[i], keys[i]*2);
            } else {
                Value val;
                table.find(keys[i], val);
            }
        }
        ops_done++;
    }
}

template<typename Table>
double benchmark(Table &table, WorkloadType workload,
                 const std::vector<Key> &keys, size_t n_threads) {
    std::vector<std::thread> threads;
    std::atomic<size_t> ops_done(0);

    size_t chunk = keys.size() / n_threads;
    auto start_time = std::chrono::high_resolution_clock::now();
    for (size_t t = 0; t < n_threads; t++) {
        size_t start = t * chunk;
        size_t end = (t == n_threads - 1) ? keys.size() : start + chunk;
        threads.emplace_back(run_workload<Table>,
                             std::ref(table),
                             workload,
                             std::cref(keys),
                             start, end,
                             std::ref(ops_done));
    }
    for (auto &th : threads) th.join();
    auto end_time = std::chrono::high_resolution_clock::now();

    double elapsed_s = std::chrono::duration<double>(end_time - start_time).count();
    return ops_done / elapsed_s; // throughput
}

// ===================== CSV Output Helper =====================
void write_csv(const std::string &filename, bool append,
               size_t dataset_size, size_t threads,
               const std::string &workload,
               double coarse_tput, double fine_tput) {
    std::ofstream file;
    if (append) file.open(filename, std::ios::app);
    else {
        file.open(filename);
        file << "Dataset,Threads,Workload,CoarseTput,FineTput\n";
    }
    file << dataset_size << "," << threads << "," << workload
         << "," << coarse_tput << "," << fine_tput << "\n";
    file.close();
}

// ===================== Main Benchmark =====================
int main() {
    std::vector<size_t> dataset_sizes = {10000, 100000, 1000000};
    std::vector<size_t> thread_counts = {1, 2, 4, 8, 16};
    std::vector<WorkloadType> workloads = {WorkloadType::LookupOnly,
                                           WorkloadType::InsertOnly,
                                           WorkloadType::Mixed7030};

    for (auto n_keys : dataset_sizes) {
        std::vector<Key> keys(n_keys);
        std::iota(keys.begin(), keys.end(), 1);

        size_t n_buckets = std::max(n_keys / 10, size_t(16));
        CoarseHashTable coarse(n_buckets);
        FineHashTable fine(n_buckets);

        for (auto workload : workloads) {
            std::string workload_str = (workload==WorkloadType::LookupOnly?"LookupOnly":
                                        workload==WorkloadType::InsertOnly?"InsertOnly":"Mixed70/30");
            std::string csv_filename = "benchmark_" + workload_str + "_" + std::to_string(n_keys) + ".csv";
            bool first_write = true;

            for (auto n_threads : thread_counts) {
                double coarse_tput = benchmark(coarse, workload, keys, n_threads);
                double fine_tput   = benchmark(fine,   workload, keys, n_threads);

                std::cout << "Dataset: " << n_keys
                          << " | Threads: " << n_threads
                          << " | Workload: " << workload_str
                          << " | Coarse Tput: " << coarse_tput
                          << " | Fine Tput: "   << fine_tput
                          << std::endl;

                write_csv(csv_filename, !first_write, n_keys, n_threads, workload_str,
                          coarse_tput, fine_tput);
                first_write = false;
            }
        }
    }
}
