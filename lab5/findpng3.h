#ifndef FINDPNG3_H
#define FINDPNG3_H

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <curl/curl.h>
#include <libxml/HTMLparser.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/uri.h>
#include <time.h>
#include <bits/getopt_core.h>
#include <sys/types.h>
#include <search.h>

#include "helpers.c"

#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576 /* 1024*1024 = 1M */
#define BUF_INC 524288   /* 1024*512  = 0.5M */
#define HASH_SIZE 1000

#define CT_PNG "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN 9
#define CT_HTML_LEN 9



#endif