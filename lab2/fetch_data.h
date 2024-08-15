#ifndef PASTER_H
#define PASTER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <curl/curl.h>
#include "concat.h"

#define SEQ_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define SEQ_COUNT 50      /* 50 unique PNGs */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

// Arguments for fetch_data() function call
typedef struct {
    char *url;
    uint8_t **buffers;
} FETCH_DATA_ARGS;

typedef struct recv_buf2 {
    char *buf;
    size_t size;
    size_t max_size;
    int seq;
} RECV_BUF;


extern pthread_mutex_t seq_mutex;
extern int seq_numbers[SEQ_COUNT];

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
int write_file(const char *path, const void *in, size_t len);
void *fetch_data(void *arg);

#endif
