#ifndef CONCAT_H
#define CONCAT_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "./starter/png_util/zutil.h"
#include "./starter/png_util/crc.h"

void concat(uint8_t** buff);

#endif 
