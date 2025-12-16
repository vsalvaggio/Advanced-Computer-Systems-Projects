// benchmark.cpp
#include <bits/stdc++.h>
#include "filters.cpp"
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <filesystem>
using namespace std;

struct Result {
    string filter;
    size_t n;
    double fpr_target;
    string workload;
    double neg_share;
    double load_factor;
    int threads;
    int fp_bits;
    double insert_s, query_s, false_pos_rate;
    double bpe;
};

vector<uint64_t> generate_keys(size_t n, uint64_t seed = 1) {
    vector<uint64_t> v(n);
    mt19937_64 rng(seed);
    for (auto &x : v) x = rng();
    return v;
}

void run_workload(Filter *f, const vector<uint64_t>& keys,
                  const vector<uint64_t>& neg_keys,
                  string workload, double neg_share,
                  Result &r) {
    size_t n = keys.size();
    size_t inserts = 0, queries = 0, fp = 0, negs = 0;

    auto start = chrono::high_resolution_clock::now();

    if (workload == "read-only") {
        for (size_t i = 0; i < n; i++) {
            uint64_t k = (i % 100 < neg_share * 100) ? neg_keys[i] : keys[i];
            bool res = f->query(k);
            if (res && i % 100 < neg_share * 100) fp++;
            queries++;
        }
    } else if (workload == "balanced") {
        for (size_t i = 0; i < n; i++) {
            if (i % 2 == 0) f->insert(keys[i]);
            else {
                uint64_t k = (i % 100 < neg_share * 100) ? neg_keys[i] : keys[i];
                bool res = f->query(k);
                if (res && i % 100 < neg_share * 100) fp++;
                queries++;
            }
        }
    } else if (workload == "read-mostly") {
        for (size_t i = 0; i < n; i++) {
            if (i % 20 == 0) f->insert(keys[i]);  // 5% inserts
            else {
                uint64_t k = (i % 100 < neg_share * 100) ? neg_keys[i] : keys[i];
                bool res = f->query(k);
                if (res && i % 100 < neg_share * 100) fp++;
                queries++;
            }
        }
    }

    auto end = chrono::high_resolution_clock::now();
    double elapsed = chrono::duration<double>(end - start).count();
    r.query_s = elapsed;
    r.false_pos_rate = (double)fp / (queries ? queries : 1);
}

int main() {
    filesystem::create_directory("results");
    vector<Result> results;

    vector<string> filters = {"quotient"};//"bloom", "xor", "cuckoo",};
    vector<size_t> sizes = {1'000'000, 5'000'000, 10'000'000};
    vector<double> fprs = {0.05, 0.01, 0.001};
    vector<string> workloads = {"read-only", "read-mostly", "balanced"};
    vector<double> neg_shares = {0.0, 0.5, 0.9};
    vector<double> load_factors = {0.4, 0.6, 0.8, 0.9, 0.95};
    vector<int> fp_bits = {8, 12, 16};
    vector<int> threads = {1, 2, 4, 8};

    for (auto n : sizes) {
        auto keys = generate_keys(n, 42);
        auto neg_keys = generate_keys(n, 999);

        for (auto fpr : fprs) {
            for (auto ftype : filters) {
                for (auto workload : workloads) {
                    for (auto neg_share : neg_shares) {
                        for (auto tcount : threads) {
                            for (auto bits : fp_bits) {
                                // Skip irrelevant params
                                if ((ftype == "xor" || ftype == "cuckoo") && bits == 0) continue;
                                if ((ftype == "xor" || ftype == "bloom") && workload != "read-only") continue; // static filters
                                if (ftype == "xor" && fpr != 0.01) continue; // just one config for brevity

                                Result r;
                                r.filter = ftype;
                                r.n = n;
                                r.fpr_target = fpr;
                                r.workload = workload;
                                r.neg_share = neg_share;
                                r.fp_bits = bits;
                                r.threads = tcount;
                                r.load_factor = 1.0;

                                unique_ptr<Filter> f(make_filter(ftype, n, fpr));
                                auto t0 = chrono::high_resolution_clock::now();
                                for (auto k : keys) f->insert(k);
                                auto t1 = chrono::high_resolution_clock::now();
                                r.insert_s = chrono::duration<double>(t1 - t0).count();

                                run_workload(f.get(), keys, neg_keys, workload, neg_share, r);

                                r.bpe = (8.0 * f->size_bytes()) / (double)r.n;
                                results.push_back(r);

                                cerr << "✅ " << ftype << " n=" << n << " fpr=" << fpr
                                     << " workload=" << workload << " neg=" << neg_share
                                     << " bits=" << bits << " threads=" << tcount
                                     << " done.\n";
                            }
                        }
                    }
                }
            }
        }
    }

    // Write CSV
    ofstream out("results/all_results.csv");
    out << "Filter,N,FPR_Target,Workload,NegShare,LoadFactor,Threads,FPBits,Insert_s,Query_s,FalsePosRate,BPE\n";
    for (auto &r : results) {
        out << r.filter << "," << r.n << "," << r.fpr_target << ","
            << r.workload << "," << r.neg_share << "," << r.load_factor << ","
            << r.threads << "," << r.fp_bits << ","
            << r.insert_s << "," << r.query_s << ","
            << r.false_pos_rate << "," << r.bpe << "\n";
    }
    cout << "✅ Results saved to results/all_results.csv\n";
}
