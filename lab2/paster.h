#ifndef PASTER_H
#define PASTER_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "concat.h"

// Arguments for fetch_data() function call
typedef struct {
    char *url;
    uint8_t **buffers;
} FETCH_DATA_ARGS;

void *fetch_data(void *arg);
void concat(uint8_t** buff);

#endif
