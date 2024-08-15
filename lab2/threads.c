#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NUM_THREADS_DEFAULT 1
#define IMG_NUM_DEFAULT 1

void *fetch_data(void *arg);  // Function declaration

int main(int argc, char **argv) {
    int num_threads = NUM_THREADS_DEFAULT;
    int img_num = IMG_NUM_DEFAULT;
    int opt;

    // Parse command-line options
    while ((opt = getopt(argc, argv, "t:n:")) != -1) {
        switch (opt) {
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'n':
                img_num = atoi(optarg);
                if (img_num < 1 || img_num > 3) {
                    fprintf(stderr, "Invalid value for -n. Valid values are 1, 2, and 3.\n");
                    return 1;
                }
                break;
            default:
                fprintf(stderr, "Usage: %s [-t num_threads] [-n img_num]\n", argv[0]);
                return 1;
        }
    }

    // Default URL if no options are specified
    if (argc == 1) {
        char url[256];
        snprintf(url, sizeof(url), "http://ece252-1.uwaterloo.ca:2520/image?img=1");
        fetch_data(url);
        return 0;
    }

    // URLs array
    char *base_urls[] = {
        "http://ece252-1.uwaterloo.ca:2520/image?img=",
        "http://ece252-2.uwaterloo.ca:2520/image?img=",
        "http://ece252-3.uwaterloo.ca:2520/image?img="
    };

    char urls[3][256];
    // Prepare the URL with the specified image number
    for (int i = 0; i < 3; i++) {
        snprintf(urls[i], sizeof(urls[i]), "%s%d", base_urls[i], img_num);
    }

    pthread_t threads[num_threads];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, fetch_data, (void *)urls[i % 3]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}