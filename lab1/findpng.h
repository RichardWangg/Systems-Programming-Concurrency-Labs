#ifndef FINDPNG_H
#define FINDPNG_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include "./starter/png_util/crc.h"
#include <netinet/in.h>

bool check_crc(FILE *f);
bool is_png(FILE *f);
char *normalize_path(const char *path); 
void check_file(char* parent, int* png_count);

#endif