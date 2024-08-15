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
#include "paster2.h"
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

void concat(IMG_DAT* shmem, RECV_BUF* segment);
void process_buffer(IMG_DAT* shmem);

#endif 
