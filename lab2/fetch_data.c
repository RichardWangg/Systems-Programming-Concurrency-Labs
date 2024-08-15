#include "fetch_data.h"

#define SEQ_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576  /* 1024*1024 = 1M */
#define BUF_INC  524288   /* 1024*512  = 0.5M */
#define SEQ_COUNT 50      /* 50 unique PNGs */

#define max(a, b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })


pthread_mutex_t seq_mutex = PTHREAD_MUTEX_INITIALIZER;
// Array to track sequence numbers
int seq_numbers[SEQ_COUNT] = {0};  

// Extract image sequence number from http header data. Returns size of header data
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata) {
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;
    if (realsize > strlen(SEQ_HEADER) &&
        strncmp(p_recv, SEQ_HEADER, strlen(SEQ_HEADER)) == 0) {
        p->seq = atoi(p_recv + strlen(SEQ_HEADER));
    }
    return realsize;
}

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata) {
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
    if (p->size + realsize + 1 > p->max_size) {
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc");
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }
    memcpy(p->buf + p->size, p_recv, realsize);
    p->size += realsize;
    p->buf[p->size] = 0;
    return realsize;
}

// Initalize the buffer with malloc
int recv_buf_init(RECV_BUF *ptr, size_t max_size) {
    void *p = NULL;
    if (ptr == NULL) {
        return 1;
    }
    p = malloc(max_size);
    if (p == NULL) {
        return 2;
    }
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;
    return 0;
}

// Deallocate buffer
int recv_buf_cleanup(RECV_BUF *ptr) {
    if (ptr == NULL) {
        return 1;
    }
    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}

void *fetch_data(void *arg) {
    FETCH_DATA_ARGS *args = (FETCH_DATA_ARGS *)arg;
    char *url = args->url;
    CURL *curl_handle;
    CURLcode res;
    RECV_BUF recv_buf;
    uint8_t** buffers = args->buffers;


    while (1) {
        recv_buf_init(&recv_buf, BUF_SIZE);
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = curl_easy_init();

        curl_easy_setopt(curl_handle, CURLOPT_URL, url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        } else {
            pthread_mutex_lock(&seq_mutex);
            if (recv_buf.seq >= 0 && recv_buf.seq < SEQ_COUNT && !seq_numbers[recv_buf.seq]) {
                seq_numbers[recv_buf.seq] = 1;
                pthread_mutex_unlock(&seq_mutex);

                printf("%lu bytes received in memory %p, seq=%d, from URL: %s\n", recv_buf.size, recv_buf.buf, recv_buf.seq, url);

                buffers[recv_buf.seq] = malloc(recv_buf.size);
                memcpy(buffers[recv_buf.seq], recv_buf.buf, recv_buf.size);

            } else {
                pthread_mutex_unlock(&seq_mutex);
            }
        }

        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        recv_buf_cleanup(&recv_buf);

        pthread_mutex_lock(&seq_mutex);
        int all_received = 1;
        for (int i = 0; i < SEQ_COUNT; i++) {
            if (!seq_numbers[i]) {
                all_received = 0;
                break;
            }
        }
        pthread_mutex_unlock(&seq_mutex);

        if (all_received) {
            break;
        }
    }
    
    return NULL;
}
