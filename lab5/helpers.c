#include "helpers.h"

#define MAX_WAIT_MSECS 30 * 1000 /* Wait max. 30 seconds */
#define BUF_INC 524288           /* 1024*512  = 0.5M */
#define ECE252_HEADER "X-Ece252-Fragment: "

#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct
{
    char **data;
    int size;
    int capacity;

    char *name; // for debugging
} UrlList;

typedef struct recv_buf2
{
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
    char* url;
} RECV_BUF;

int is_png(char *buf)
{
    const unsigned char png_signature[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    return memcmp(buf, png_signature, 8) == 0;
}

void url_list_init(UrlList *list)
{
    list->data = malloc(sizeof(char *) * 10);
    list->size = 0;
    list->capacity = 10;
}

void url_list_add(UrlList *list, const char *url)
{
    if (list->size == list->capacity)
    {
        list->capacity *= 2;
        list->data = realloc(list->data, sizeof(char *) * list->capacity);
    }
    list->data[list->size] = strdup(url);
    list->size++;

    // printf("Adding %s to %s... Current list size: %d\n", url, list->name, list->size);
}

char *url_list_pop(UrlList *list)
{
    if (list->size == 0)
    {
        return NULL;
    }
    char *url = list->data[0];
    memmove(list->data, list->data + 1, sizeof(char *) * (list->size - 1));
    list->size--;

    // printf("Popping %s from %s... Current list size: %d\n", url, list->name, list->size);

    return url;
}

int url_list_contains(UrlList *list, const char *url)
{
    for (int i = 0; i < list->size; i++)
    {
        if (strcmp(list->data[i], url) == 0)
        {
            // printf("%s found in %s. Skipping...\n", url, list->name);
            return 1;
        }
    }
    return 0;
}

void url_list_free(UrlList *list)
{
    for (int i = 0; i < list->size; i++)
    {
        free(list->data[i]);
    }
    free(list->data);
}

htmlDocPtr mem_getdoc(char *buf, int size, const char *url)
{
    int opts = HTML_PARSE_NOBLANKS | HTML_PARSE_NOERROR |
               HTML_PARSE_NOWARNING | HTML_PARSE_NONET;
    htmlDocPtr doc = htmlReadMemory(buf, size, url, NULL, opts);

    if (doc == NULL)
    {
        fprintf(stderr, "Document not parsed successfully.\n");
        return NULL;
    }
    return doc;
}

xmlXPathObjectPtr getnodeset(xmlDocPtr doc, xmlChar *xpath)
{

    xmlXPathContextPtr context;
    xmlXPathObjectPtr result;

    context = xmlXPathNewContext(doc);
    if (context == NULL)
    {
        printf("Error in xmlXPathNewContext\n");
        return NULL;
    }
    result = xmlXPathEvalExpression(xpath, context);
    xmlXPathFreeContext(context);
    if (result == NULL)
    {
        printf("Error in xmlXPathEvalExpression\n");
        return NULL;
    }
    if (xmlXPathNodeSetIsEmpty(result->nodesetval))
    {
        xmlXPathFreeObject(result);
        printf("No result\n");
        return NULL;
    }
    return result;
}

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

#ifdef DEBUG1_
    printf("%s", p_recv);
#endif /* DEBUG1_ */
    if (realsize > strlen(ECE252_HEADER) &&
        strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0)
    {

        /* extract img sequence number */
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }
    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;
    
    if (ptr == NULL) {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL) {
	    return 2;
    }
    
    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1;              /* valid seq should be positive */
    return 0;
}

static size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;
 
    if (p->size + realsize + 1 > p->max_size) {/* hope this rarely happens */ 
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);   
        char *q = realloc(p->buf, new_size);
        if (q == NULL) {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

void write_hash_table_entry(const char *key, void *value, void *user_data) {
    FILE *log_file = (FILE *)user_data;
    fprintf(log_file, "%s\n", key);
}