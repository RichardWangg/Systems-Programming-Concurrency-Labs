#ifndef PNGINFO_H
#define PNGINFO_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../starter/png_util/crc.h"
#include <netinet/in.h>

bool is_png(FILE *f);
void get_png_data_IHDR(FILE *f, uint32_t *h, uint32_t *w);
void check_chunks(FILE *f);

#endif