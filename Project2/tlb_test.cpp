// tlb_test.cpp
#include <bits/stdc++.h>
#include <chrono>
using namespace std;
using ns = chrono::duration<double>;
int main(int argc, char **argv){
    if(argc < 4){ fprintf(stderr,"usage: %s <num_pages> <page_size> <iters>\n", argv[0]); return 1; }
    size_t num_pages = stoull(argv[1]);
    size_t page_size = stoull(argv[2]); // e.g., 4096 or 2097152
    size_t iters = stoull(argv[3]);

    size_t N = num_pages * page_size;
    vector<char> buf(N);
    // touch once to fault in pages
    for(size_t p=0;p<num_pages;++p){
        buf[p*page_size] = 1;
    }
    volatile uint64_t sink=0;
    auto t0 = chrono::high_resolution_clock::now();
    for(size_t i=0;i<iters;i++){
        size_t p = i % num_pages;
        sink += buf[p*page_size];
    }
    auto t1 = chrono::high_resolution_clock::now();
    double sec = chrono::duration_cast<ns>(t1-t0).count();
    printf("pages=%zu page_size=%zu iters=%zu sec=%.6f sink=%lu\n", num_pages, page_size, iters, sec, (unsigned long)sink);
    return 0;
}