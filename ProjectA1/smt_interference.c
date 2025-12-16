#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N (2L*500*1000*1000) // 1 billion iterations per thread

double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec/1e9;
}

// Workload: heavy floating-point loop
void* work(void* arg){
    volatile double x = 0;
    for(long i = 0; i < N; i++)
        x += i * 0.0000001;
    return NULL;
}

// Pin a pthread_attr to a single CPU
void set_affinity(pthread_attr_t* attr, int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    pthread_attr_setaffinity_np(attr, sizeof(set), &set);
}

int main() {
    pthread_t t1, t2;
    pthread_attr_t attr1, attr2;
    double t0, t1s;

    // -------------------------------
    // Mode 1: Both threads on SAME core
    // -------------------------------
    pthread_attr_init(&attr1);
    pthread_attr_init(&attr2);
    set_affinity(&attr1, 0);
    set_affinity(&attr2, 0); // same logical core -> SMT contention

    t0 = now_sec();
    pthread_create(&t1, &attr1, work, NULL);
    pthread_create(&t2, &attr2, work, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    t1s = now_sec();
    printf("shared_core,%.3f\n", t1s - t0);

    // -------------------------------
    // Mode 2: Threads on DIFFERENT cores
    // -------------------------------
    pthread_attr_init(&attr1);
    pthread_attr_init(&attr2);
    set_affinity(&attr1, 0);
    set_affinity(&attr2, 1); // separate cores

    t0 = now_sec();
    pthread_create(&t1, &attr1, work, NULL);
    pthread_create(&t2, &attr2, work, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    t1s = now_sec();
    printf("separate_core,%.3f\n", t1s - t0);

    return 0;
}
