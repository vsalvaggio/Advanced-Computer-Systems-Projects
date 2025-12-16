#define _GNU_SOURCE
#include <aio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>


#define FILE_SIZE (100 * 1024 * 1024)
#define BLOCK 4096

double now_sec(){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return ts.tv_sec+ts.tv_nsec/1e9;}

void make_file(const char* f){
    int fd=open(f,O_CREAT|O_WRONLY|O_TRUNC,0644);
    char buf[BLOCK]; memset(buf,'A',sizeof(buf));
    for(size_t i=0;i<FILE_SIZE/BLOCK;i++) write(fd,buf,sizeof(buf));
    close(fd);
}

int main(){
    const char* f="aio_test.dat";
    make_file(f);
    int fd=open(f,O_RDONLY);
    char buf[BLOCK];
    struct aiocb cb; memset(&cb,0,sizeof(cb));
    cb.aio_fildes=fd; cb.aio_buf=buf; cb.aio_nbytes=BLOCK;

    // synchronous
    double t0=now_sec();
    for(size_t off=0; off<FILE_SIZE; off+=BLOCK)
        pread(fd,buf,BLOCK,off);
    double t1=now_sec();
    printf("sync_io,%.3f\n",t1-t0);

    // asynchronous
    t0=now_sec();
    for(size_t off=0; off<FILE_SIZE; off+=BLOCK){
        cb.aio_offset=off;
        aio_read(&cb);
        while(aio_error(&cb)==EINPROGRESS);
        aio_return(&cb);
    }
    t1=now_sec();
    printf("async_aio,%.3f\n",t1-t0);
    close(fd);
    return 0;
}
