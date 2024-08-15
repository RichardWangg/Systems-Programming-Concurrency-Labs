#ifndef PASTER2_H
#define PASTER2_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define BUF_SIZE 1048576 
#define NUM_PARTS 50
#define ECE252_HEADER "X-Ece252-Fragment: "

int is_png(char *buf);


typedef struct {
    size_t size;     /* size of valid data in buf in bytes */
    size_t max_size; /* max capacity of buf in bytes */
    int seq;         /* >=0 sequence number extracted from http header */
    char buf[BUF_SIZE]; /* buffer to hold the data */
} RECV_BUF;

typedef struct {
    int in;
    int out;
    int count;
    int produced_count;
    int consumed_count;
    int done;
    sem_t mutex;
    sem_t full;
    sem_t empty;
    RECV_BUF buffer[]; /* Flexible array member for the buffer slots */
} SharedBuffer;

typedef struct IMG_DAT {
    unsigned int width;
    unsigned int long uncompressed_IDAT_total;
    unsigned int long uncompressed_IDAT[50];
    unsigned int total_height;
    unsigned int total_IDAT_size;
    RECV_BUF IDATdata[50];
} IMG_DAT;

#endif
