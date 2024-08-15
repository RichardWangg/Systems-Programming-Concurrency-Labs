#ifndef CATPNG_H
#define CATPNG_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "./starter/png_util/zutil.h"
#include <netinet/in.h>

void get_png_data_IHDR(FILE *f, uint32_t *h, uint32_t *w);

#endif