#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N (2L*1000*1000*100) // 200 million iterations per thread
#define NUM_THREADS 2

double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/1e9;
}

// Workload that cannot be optimized away
void* work(void* arg) {
    double x = 0.0;
    for(long i = 0; i < N; i++) {
        x += i * 0.0000001;
        x = x / 1.0000001 + 0.0000001; // extra dependency
    }
    // prevent compiler from removing the loop
    volatile double sink = x;
    (void)sink;
    return NULL;
}

// pin a pthread_attr to a CPU
void set_affinity(pthread_attr_t* attr, int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_attr_setaffinity_np(attr, sizeof(set), &set);
}

int main() {
    pthread_t threads[NUM_THREADS];
    pthread_attr_t attrs[NUM_THREADS];
    double t0, t1s;

    // -------------------------------
    // Mode 1: no affinity
    // -------------------------------
    for(int i = 0; i < NUM_THREADS; i++)
        pthread_attr_init(&attrs[i]);

    t0 = now_sec();
    for(int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], &attrs[i], work, NULL);

    for(int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    t1s = now_sec();
    printf("no_affinity,%.3f\n", t1s - t0);

    // -------------------------------
    // Mode 2: pinned to specific cores
    // -------------------------------
    for(int i = 0; i < NUM_THREADS; i++) {
        pthread_attr_init(&attrs[i]);
        set_affinity(&attrs[i], i); // thread i -> core i
    }

    t0 = now_sec();
    for(int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], &attrs[i], work, NULL);

    for(int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);
    t1s = now_sec();
    printf("pinned_affinity,%.3f\n", t1s - t0);

    return 0;
}
