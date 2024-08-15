#include "paster.h"
// Lab 2

int main(int argc, char **argv) {
    int num_threads = 1;
    int img_num = 1;
    int opt;

    // Array to hold png filepaths
    uint8_t** png_buffers = malloc(50 * sizeof(uint8_t*));

    // Read options
    while ((opt = getopt(argc, argv, "t:n:")) != -1) {
        if (opt == 't') {
            num_threads = atoi(optarg);
        } else if (opt == 'n') {
            img_num = atoi(optarg);
            if (img_num < 1 || img_num > 3) {
                fprintf(stderr, "Incorrect n value. n is [1,3]\n");
                return 1;
            }
        } else if (opt == '?') {
            fprintf(stderr, "Unknown option '%c'\n", optopt);
            return 1;
        }
    }
    
    // Default URL if no options
    if (argc == 1) {
        char url[256];
        snprintf(url, sizeof(url), "http://ece252-1.uwaterloo.ca:2520/image?img=1");
        FETCH_DATA_ARGS args;
        args.url = url;
        args.buffers = png_buffers;
        fetch_data((void *)&args);
        concat(png_buffers);
        return 0;
    }

    // If there is no -t option or -t = 1
    if (num_threads == 1) {
        char url[256];
        snprintf(url, sizeof(url), "http://ece252-1.uwaterloo.ca:2520/image?img=%d", img_num);
        FETCH_DATA_ARGS args;
        args.url = url;
        args.buffers = png_buffers;
        fetch_data((void *)&args);
        concat(png_buffers);
        return 0;
    }

    // URLs array
    char *base_urls[] = {
        "http://ece252-1.uwaterloo.ca:2520/image?img=",
        "http://ece252-2.uwaterloo.ca:2520/image?img=",
        "http://ece252-3.uwaterloo.ca:2520/image?img="
    };

    char urls[3][256];
    // Add the image number to URL
    for (int i = 0; i < 3; i++) {
        snprintf(urls[i], sizeof(urls[i]), "%s%d", base_urls[i], img_num);
    }
    
    pthread_t threads[num_threads];

    // Create threads
    FETCH_DATA_ARGS args[num_threads];
    for (int i = 0; i < num_threads; i++) {
        args[i].url = urls[i % 3];
        args[i].buffers = png_buffers;
        printf("Creating thread %d\n", i);
        if (pthread_create(&threads[i], NULL, fetch_data, (void *)&args[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    // Join threads
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Concatenate the pngs
    concat(png_buffers);

    for (int i = 0; i < 50; i++) {
        free(png_buffers[i]);
    }
    free(png_buffers);

    return 0;
}
