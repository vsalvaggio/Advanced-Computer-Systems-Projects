#define _GNU_SOURCE
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define FILE_SIZE (100 * 1024 * 1024) // 100 MB

double now_sec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void make_file(const char* f) {
    int fd = open(f, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    char buf[4096]; memset(buf, 'A', sizeof(buf));
    for (size_t i = 0; i < FILE_SIZE/sizeof(buf); i++)
        write(fd, buf, sizeof(buf));
    close(fd);
}

void* drain(void* arg){
    int s = *(int*)arg; char buf[4096];
    while (read(s, buf, sizeof(buf)) > 0);
    return NULL;
}

int main() {
    const char* f = "testfile.dat";
    make_file(f);
    struct stat st;
    int fd = open(f, O_RDONLY);
    fstat(fd, &st);
    close(fd);

    // Regular read
    fd = open(f, O_RDONLY);
    char buf[4096]; double t0 = now_sec();
    while (read(fd, buf, sizeof(buf)) > 0);
    double t1 = now_sec();
    printf("regular_read,%.3f\n", t1 - t0);
    close(fd);

    // Zero-copy sendfile
    int sv[2]; socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    pthread_t dr; pthread_create(&dr, NULL, drain, &sv[1]);
    fd = open(f, O_RDONLY); off_t off = 0;
    t0 = now_sec();
    sendfile(sv[0], fd, &off, st.st_size);
    t1 = now_sec();
    printf("sendfile,%.3f\n", t1 - t0);
    shutdown(sv[0], SHUT_WR);
    pthread_join(dr, NULL);
    close(fd); close(sv[0]); close(sv[1]);
    return 0;
}
