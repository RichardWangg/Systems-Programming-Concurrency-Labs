#include "paster2.h"
#include "concat.h"

const char *IMG_URLS[] = {
    "http://ece252-1.uwaterloo.ca:2530/image?img=%d&part=%d",
    "http://ece252-2.uwaterloo.ca:2530/image?img=%d&part=%d",
    "http://ece252-3.uwaterloo.ca:2530/image?img=%d&part=%d"};

int is_png(char *buf) {
    const unsigned char png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return memcmp(buf, png_signature, 8) == 0;
}

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata) {
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) && strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0) {
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }
    return realsize;
}

size_t write_cb_curl(char *p_recv, size_t size, size_t nmemb, void *p_userdata) {
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size) {
        fprintf(stderr, "User buffer is too small, abort...\n");
        abort();
    }

    memcpy(p->buf + p->size, p_recv, realsize);
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int write_file(const char *path, const void *in, size_t len)
{
    FILE *fp = NULL;

    if (path == NULL) {
        fprintf(stderr, "write_file: file name is null!\n");
        return -1;
    }

    if (in == NULL) {
        fprintf(stderr, "write_file: input data is null!\n");
        return -1;
    }

    fp = fopen(path, "wb");
    if (fp == NULL) {
        perror("fopen");
        return -2;
    }

    if (fwrite(in, 1, len, fp) != len) {
        fprintf(stderr, "write_file: incomplete write!\n");
        return -3; 
    }
    return fclose(fp);
}

void producer(int id, int num_producers, int img_num, SharedBuffer *shared_buffer, int buffer_capacity) {
    CURL *curl_handle;
    CURLcode res;
    char url[256];

    for (int i = id; i < NUM_PARTS; i += num_producers) {
        const char *server_url = IMG_URLS[id % 3];
        sprintf(url, server_url, img_num, i);

        RECV_BUF segment;
        segment.size = 0;
        segment.max_size = BUF_SIZE;
        segment.seq = -1;

        curl_handle = curl_easy_init();
        if (curl_handle) {
            curl_easy_setopt(curl_handle, CURLOPT_URL, url);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl);
            curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &segment);
            curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);
            curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, &segment);
            curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
            res = curl_easy_perform(curl_handle);
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            } else {
                // printf("Producer %d fetched part %d, size %lu bytes\n", id, i, segment.size);
                // // Save the segment to a file
                // char filename[256];
                // sprintf(filename, "part_%d.png", segment.seq);
                // if (write_file(filename, segment.buf, segment.size) != 0) {
                //     fprintf(stderr, "Failed to write part %d to file\n", segment.seq);
                // } else {
                //     printf("Producer %d saved part %d to file %s\n", id, segment.seq, filename);
                // }
            }
            curl_easy_cleanup(curl_handle);
            // printf("Producer %d is waiting with part %d ...\n", id, i);
            sem_wait(&shared_buffer->empty);
            // printf("Producer %d FINISHED waiting\n", id);
            sem_wait(&shared_buffer->mutex);
            // printf("Producer %d unlocked mutex\n", id);

            int index = shared_buffer->in;
            shared_buffer->buffer[index] = segment;
            // printf("Producer %d adding to shared buffer at index %d: ", id, index);
            shared_buffer->in = (shared_buffer->in + 1) % buffer_capacity;
            shared_buffer->count++;
            shared_buffer->produced_count++;

            // printf("Producer %d added part %d to buffer at position %d\n", id, i, (shared_buffer->in - 1 + buffer_capacity) % buffer_capacity);

            sem_post(&shared_buffer->mutex);
            sem_post(&shared_buffer->full);
        }
    }
    // printf("Producer %d: exiting\n", id);
    exit(0);
}

void consumer(int id, int sleep_time, SharedBuffer *shared_buffer, int buffer_capacity, IMG_DAT* output_shmem) {
    while (1) {
        //printf("Consumer %d is waiting...\n", id);
        sem_wait(&shared_buffer->full);
        //printf("Consumer %d FINISHED waiting\n", id);
        sem_wait(&shared_buffer->mutex);

        if (shared_buffer->count > 0) {
            int index = shared_buffer->out;
            RECV_BUF segment = shared_buffer->buffer[index];
            shared_buffer->out = (shared_buffer->out + 1) % buffer_capacity;
            shared_buffer->count--;
            shared_buffer->consumed_count++;

            concat(output_shmem, &segment);

            // printf("Consumer %d consumed part %d, size %lu bytes from position %d\n", id, segment.seq, segment.size, (shared_buffer->out - 1 + buffer_capacity) % buffer_capacity);
           // printf("CONSUMED COUNT is now: %d\n", shared_buffer->consumed_count);
            if (shared_buffer->consumed_count == NUM_PARTS) {
                // printf("we have consumed everything \n");
                shared_buffer->done = 1;
                sem_post(&shared_buffer->mutex);
                sem_post(&shared_buffer->full);
                break;
            }

            sem_post(&shared_buffer->mutex);
            sem_post(&shared_buffer->empty);

            usleep(sleep_time * 1000);
        } else if (shared_buffer->done) {
            // printf("Consumer %d: all parts consumed, exiting\n", id);
            sem_post(&shared_buffer->mutex);
            sem_post(&shared_buffer->full);
            break;
        } else {
            // printf("Consumer %d: nothing to consume, waiting\n", id);
            sem_post(&shared_buffer->mutex);
            sem_post(&shared_buffer->full);
        }
    }

    // printf("Consumer %d: exiting\n", id);
    exit(0);
}


int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s <B> <P> <C> <X> <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int buffer_capacity = atoi(argv[1]);
    int num_producers = atoi(argv[2]);
    int num_consumers = atoi(argv[3]);
    int sleep_time = atoi(argv[4]);
    int img_num = atoi(argv[5]);

    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedBuffer) + buffer_capacity * sizeof(RECV_BUF), IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    SharedBuffer *shared_buffer = shmat(shm_id, NULL, 0);
    if (shared_buffer == (void *)-1) {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    // Initialize shared buffer and synchronization primitives
    shared_buffer->in = 0;
    shared_buffer->out = 0;
    shared_buffer->count = 0;
    shared_buffer->produced_count = 0;
    shared_buffer->consumed_count = 0;
    shared_buffer->done = 0;
    sem_init(&(shared_buffer->mutex), 1, 1);  // Binary semaphore for mutual exclusion
    sem_init(&(shared_buffer->full), 1, 0);
    sem_init(&(shared_buffer->empty), 1, buffer_capacity);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    pid_t pids[num_producers + num_consumers];

    struct timeval start, end;
    gettimeofday(&start, NULL);

    int64_t shmid_imgdat = shmget(IPC_PRIVATE, sizeof(IMG_DAT), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);
    IMG_DAT* output_shmem = shmat(shmid_imgdat, NULL, 0);

    for (int i = 0; i < num_producers; i++) {
        if ((pids[i] = fork()) == 0) {
            producer(i, num_producers, img_num, shared_buffer, buffer_capacity);
        }
    }

    for (int i = 0; i < num_consumers; i++) {
        if ((pids[num_producers + i] = fork()) == 0) {
            consumer(i, sleep_time, shared_buffer, buffer_capacity, output_shmem);
        }
    }

    for (int i = 0; i < num_producers + num_consumers; i++) {
        waitpid(pids[i], NULL, 0);
    }

    process_buffer(output_shmem);

    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec - start.tv_sec) * 1e6;
    time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e-6;
    printf("paster2 execution time: %.6f seconds\n", time_taken);

    sem_destroy(&(shared_buffer->mutex));
    sem_destroy(&(shared_buffer->full));
    sem_destroy(&(shared_buffer->empty));

    shmdt(shared_buffer);
    shmctl(shm_id, IPC_RMID, NULL);

    curl_global_cleanup();

    return 0;
}
