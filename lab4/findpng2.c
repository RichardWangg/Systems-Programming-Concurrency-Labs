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
#include "helpers.h"

#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576 /* 1024*1024 = 1M */
#define BUF_INC 524288   /* 1024*512  = 0.5M */

#define CT_PNG "image/png"
#define CT_HTML "text/html"
#define CT_PNG_LEN 9
#define CT_HTML_LEN 9

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    char **data;
    int size;
    int capacity;
} UrlList;

typedef struct
{
    UrlList frontier;
    UrlList visited;
    UrlList png_urls;
    int png_count;
    int max_pngs;
    int waiting_threads;
    int num_threads;
    int terminate;
    pthread_mutex_t count_mutex;
    pthread_mutex_t waiting_mutex;
    pthread_cond_t terminate_cond;
    char *logfile;
} SharedData;

int is_png(char *buf)
{
    const unsigned char png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return memcmp(buf, png_signature, 8) == 0;
}

void url_list_init(UrlList *list)
{
    pthread_mutex_init(&list->mutex, NULL);
    pthread_cond_init(&list->cond, NULL);
    list->data = malloc(sizeof(char *) * 10);
    list->size = 0;
    list->capacity = 10;
}

void url_list_add(UrlList *list, const char *url)
{
    pthread_mutex_lock(&list->mutex);
    if (list->size == list->capacity)
    {
        list->capacity *= 2;
        list->data = realloc(list->data, sizeof(char *) * list->capacity);
    }
    list->data[list->size] = strdup(url);
    list->size++;
    pthread_cond_signal(&list->cond);
    pthread_mutex_unlock(&list->mutex);
}

char *url_list_pop(UrlList *list, SharedData *data)
{
    pthread_mutex_lock(&list->mutex);

    while (list->size == 0 && !data->terminate)
    {
        pthread_mutex_lock(&data->waiting_mutex);
        data->waiting_threads++;

        // Check if all threads are waiting
        if (data->waiting_threads == data->num_threads)
        {
            pthread_mutex_lock(&data->count_mutex);
            data->terminate = 1;
            pthread_mutex_unlock(&data->count_mutex);
            pthread_cond_broadcast(&list->cond); // Signal all waiting threads to terminate
        }

        pthread_mutex_unlock(&data->waiting_mutex);

        if (!data->terminate) // Avoid waiting if terminate flag is already set
        {
            pthread_cond_wait(&list->cond, &list->mutex);
        }
        if (data->terminate)
        {
            pthread_mutex_unlock(&list->mutex);
            return NULL;
        }

        pthread_mutex_lock(&data->waiting_mutex);
        data->waiting_threads--;
        pthread_mutex_unlock(&data->waiting_mutex);
    }

    char *url = list->data[0];
    memmove(list->data, list->data + 1, sizeof(char *) * (list->size - 1));
    list->size--;
    pthread_mutex_unlock(&list->mutex);
    return url;
}

int url_list_contains(UrlList *list, const char *url)
{
    pthread_mutex_lock(&list->mutex);
    for (int i = 0; i < list->size; i++)
    {
        if (strcmp(list->data[i], url) == 0)
        {
            pthread_mutex_unlock(&list->mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&list->mutex);
    return 0;
}

void url_list_free(UrlList *list)
{
    for (int i = 0; i < list->size; i++)
    {
        free(list->data[i]);
    }
    free(list->data);
    pthread_mutex_destroy(&list->mutex);
    pthread_cond_destroy(&list->cond);
}

int find_http(char *buf, int size, int follow_relative_links, const char *base_url, UrlList *frontier)
{
    int i;
    htmlDocPtr doc;
    xmlChar *xpath = (xmlChar *)"//a/@href";
    xmlNodeSetPtr nodeset;
    xmlXPathObjectPtr result;
    xmlChar *href;

    if (buf == NULL)
    {
        return 1;
    }

    doc = mem_getdoc(buf, size, base_url);
    result = getnodeset(doc, xpath);
    if (result)
    {
        nodeset = result->nodesetval;
        printf("Iterating %d times\n", nodeset->nodeNr);
        for (i = 0; i < nodeset->nodeNr; i++)
        {
            href = xmlNodeListGetString(doc, nodeset->nodeTab[i]->xmlChildrenNode, 1);
            if (follow_relative_links)
            {
                xmlChar *old = href;
                href = xmlBuildURI(href, (xmlChar *)base_url);
                xmlFree(old);
            }
            if (href != NULL && strncmp((const char *)href, "http", 4) == 0)
            {
                char *url_str = (char *)href;
                url_list_add(frontier, url_str); // Add URL to frontier
            }
            xmlFree(href);
        }
        xmlXPathFreeObject(result);
    }
    xmlFreeDoc(doc);
    return 0;
}

void *crawl(void *arg)
{
    SharedData *data = (SharedData *)arg;

    while (1)
    {
        // Terminate thread if png count is = max count
        pthread_mutex_lock(&data->count_mutex);
        if (data->png_count >= data->max_pngs)
        {
            data->terminate = 1;
            pthread_mutex_unlock(&data->count_mutex);
            pthread_cond_broadcast(&data->frontier.cond);
            break;
        }
        pthread_mutex_unlock(&data->count_mutex);

        // Terminate if all threads are waiting

        char *url = url_list_pop(&data->frontier, data);
        if (url == NULL)
        {
            printf("terminating thread\n");
            break;
        }

        // Check if visited already
        if (url_list_contains(&data->visited, url))
        {
            free(url);
            continue;
        }

        // Add url to visited
        url_list_add(&data->visited, url);

        // Start CURL
        CURL *curl_handle;
        CURLcode res;
        RECV_BUF recv_buf;

        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_handle = easy_handle_init(&recv_buf, url);
        if (curl_handle == NULL)
        {
            fprintf(stderr, "Curl initialization failed. Exiting...\n");
            curl_global_cleanup();
            abort();
        }

        // Enable following redirects
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);

        res = curl_easy_perform(curl_handle);
        if (res != CURLE_OK)
        {
            free(url);
            cleanup(curl_handle, &recv_buf);
            continue;
        }
        // Check if content type is PNG, add URL to hash table if it is
        char *content_type = NULL;
        res = curl_easy_getinfo(curl_handle, CURLINFO_CONTENT_TYPE, &content_type);
        if ((res == CURLE_OK) && content_type)
        {
            if (strcmp(content_type, "image/png") == 0)
            {
                printf("Content-Type is image/png. URL: %s\n", url);
                if (is_png(recv_buf.buf))
                {
                    pthread_mutex_lock(&data->count_mutex);
                    data->png_count++;
                    pthread_mutex_unlock(&data->count_mutex);
                    url_list_add(&data->png_urls, url);
                }
            }
            if (strcmp(content_type, "text/html") == 0)
            {
                printf("Content-Type is text/html. URL: %s\n", url);
            }
        }

        // Add new URLs
        find_http(recv_buf.buf, recv_buf.size, 1, url, &data->frontier);
        cleanup(curl_handle, &recv_buf);
        free(url);
    }

    curl_global_cleanup();
    return NULL;
}

int main(int argc, char **argv)
{
    int num_threads = 1;
    int max_pngs = 50;
    char *logfile = NULL;
    xmlInitParser();

    int opt;
    while ((opt = getopt(argc, argv, "t:m:v:p:")) != -1)
    {
        if (opt == 't')
        {
            num_threads = atoi(optarg);
        }
        else if (opt == 'm')
        {
            max_pngs = atoi(optarg);
        }
        else if (opt == 'v')
        {
            logfile = malloc(strlen(optarg) + 1);
            strcpy(logfile, optarg);
        }
        else if (opt == '?')
        {
            fprintf(stderr, "Unknown option '-%c'\n", optopt);
            return 1;
        }
    }
    int print_pngs = max_pngs;
    if (max_pngs > 50)
    {
        print_pngs = 50;
    }

    if (optind >= argc)
    {
        fprintf(stderr, "SEED URL not provided\n");
        return 1;
    }
    char *seed_url = argv[optind];

    SharedData data;
    url_list_init(&data.frontier);
    url_list_init(&data.visited);
    url_list_init(&data.png_urls);
    data.png_count = 0;
    data.max_pngs = max_pngs;
    data.logfile = logfile;
    data.waiting_threads = 0;
    data.num_threads = num_threads;
    data.terminate = 0;
    pthread_mutex_init(&data.count_mutex, NULL);
    pthread_mutex_init(&data.waiting_mutex, NULL);
    pthread_cond_init(&data.terminate_cond, NULL);

    url_list_add(&data.frontier, seed_url);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        pthread_create(&threads[i], NULL, crawl, &data);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    FILE *png_file = fopen("png_urls.txt", "w");
    if (png_file)
    {
        int limit = print_pngs < data.png_urls.size ? print_pngs : data.png_urls.size;
        // printf("png url size: %d\n", data.png_urls.size);
        // printf("print png size: %d\n", print_pngs);
        // printf("limit is: %d\n", limit);
        for (int i = 0; i < limit; i++)
        {
            fprintf(png_file, "%s\n", data.png_urls.data[i]);
        }
        fclose(png_file);
    }

    if (logfile)
    {
        FILE *log_file = fopen(logfile, "w");
        if (log_file)
        {
            for (int i = 0; i < data.visited.size; i++)
            {
                fprintf(log_file, "%s\n", data.visited.data[i]);
            }
            fclose(log_file);
        }
    }

    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec - start.tv_sec) * 1e6;
    time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e-6;
    printf("findpng2 execution time: %.6f seconds\n", time_taken);

    url_list_free(&data.frontier);
    url_list_free(&data.visited);
    url_list_free(&data.png_urls);
    pthread_mutex_destroy(&data.count_mutex);
    pthread_mutex_destroy(&data.waiting_mutex);
    pthread_cond_destroy(&data.terminate_cond);
    xmlCleanupParser();
    free(logfile);

    return 0;
}
