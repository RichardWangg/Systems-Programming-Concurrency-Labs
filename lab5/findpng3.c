#include "findpng3.h"

UrlList frontier;
UrlList visited;
UrlList png_urls;

CURL* buffs[1000];
int buffs_count = 0;

char* buffs_data[1000];
int data_count = 0;

int png_count = 0;
int frontier_empty = 0;
int max_connections = 1;
int max_pngs = 50;

static size_t cb(char *p_recv, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    RECV_BUF *mem = (RECV_BUF *)userp;

    if (mem->size + realsize + 1 > mem->max_size)
    {
        size_t new_size = mem->max_size + (realsize > BUF_INC ? realsize : BUF_INC);
        char *q = realloc(mem->buf, new_size);
        if (q == NULL)
        {
            perror("realloc");
            return -1;
        }
        mem->buf = q;
        mem->max_size = new_size;
    }
    memcpy(mem->buf + mem->size, p_recv, realsize);
    // memcpy(&(mem->buf[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->buf[mem->size] = 0;

    return realsize;
}

static void init(CURLM *cm, RECV_BUF *recv_buf, const char *url)
{
    CURL *eh = curl_easy_init();

    // recv_buf->buf = malloc(BUF_INC);
    // if (recv_buf->buf == NULL)
    // {
    //     perror("malloc");
    //     exit(EXIT_FAILURE);
    // }
    // recv_buf->size = 0;
    // recv_buf->max_size = BUF_INC;

    curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, cb);
    curl_easy_setopt(eh, CURLOPT_HEADER, 0L);
    curl_easy_setopt(eh, CURLOPT_URL, url);
    curl_easy_setopt(eh, CURLOPT_WRITEDATA, (void *)recv_buf);
    curl_easy_setopt(eh, CURLOPT_PRIVATE, recv_buf);
    curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(eh, CURLOPT_UNRESTRICTED_AUTH, 1L);
    curl_easy_setopt(eh, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(eh, CURLOPT_ACCEPT_ENCODING, "");
    curl_easy_setopt(eh, CURLOPT_COOKIEFILE, "");
    curl_easy_setopt(eh, CURLOPT_PROXYAUTH, CURLAUTH_ANY);
    curl_easy_setopt(eh, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    curl_easy_setopt(eh, CURLOPT_HEADERFUNCTION, header_cb_curl);
    curl_easy_setopt(eh, CURLOPT_HEADERDATA, (void *)recv_buf);
    curl_easy_setopt(eh, CURLOPT_VERBOSE, 0L);

    curl_multi_add_handle(cm, eh);
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
            char *url_str = (char *)href;
            if (href != NULL && strncmp((const char *)href, "http", 4) == 0 && !url_list_contains(&visited, url_str))
            {
                url_list_add(frontier, url_str); // Add URL to frontier
            }
            xmlFree(href);
        }
        xmlXPathFreeObject(result);
    }
    xmlFreeDoc(doc);
    return 0;
}

void crawl()
{
    CURLM *cm = curl_multi_init();
    curl_global_init(CURL_GLOBAL_ALL);
    CURL *eh = NULL;
    CURLMsg *msg = NULL;
    CURLcode return_code = 0;
    int still_running = 0, msgs_left = 0;
    while (1)
    {
        int num_connections = 0;

        while(num_connections < max_connections){
            char* url = url_list_pop(&frontier);
            if (url == NULL) {
                frontier_empty = 1;
                break;
            }

            if (url_list_contains(&visited, url)) {
                free(url);
                continue;
            }
            ++num_connections;
            RECV_BUF *recv_data = malloc(sizeof(RECV_BUF));

            buffs[buffs_count] = recv_data;
            ++buffs_count;

            url_list_add(&visited, url);

            recv_data->size = 0;
            recv_data->max_size = BUF_SIZE;
            recv_data->seq = -1;
            recv_data->url = url;
            recv_data->buf = (char *) malloc(BUF_SIZE);

            buffs_data[data_count] = recv_data->buf;
            ++data_count;

            init(cm, recv_data, url);
            curl_multi_perform(cm, &still_running);
            
            free(url);
        }

        do
        {
            // printf("%d\n", num_connections);
            int numfds = 0;
            int res = curl_multi_wait(cm, NULL, 0, MAX_WAIT_MSECS, &numfds);
            if (res != CURLM_OK)
            {
                fprintf(stderr, "error: curl_multi_wait() returned %d\n", res);
                return;
            }
            curl_multi_perform(cm, &still_running);
        } while(still_running);

        if (png_urls.size == max_pngs) {
            while ((msg = curl_multi_info_read(cm, &msgs_left))) {
                eh = msg->easy_handle;
                curl_multi_remove_handle(cm, eh);
                curl_easy_cleanup(eh);
            }
            break;
        }

        while ((msg = curl_multi_info_read(cm, &msgs_left)))
        {
            if (msg->msg == CURLMSG_DONE)
            {
                eh = msg->easy_handle;

                return_code = msg->data.result;
                if (return_code != CURLE_OK)
                {
                    fprintf(stderr, "CURL error code: %d\n", msg->data.result);
                    curl_multi_remove_handle(cm, eh);
                    curl_easy_cleanup(eh);
                    continue;
                }
                else {
                    RECV_BUF *recv_buf = NULL;
                    curl_easy_getinfo(eh, CURLINFO_PRIVATE, &recv_buf);

                    char *content_type = NULL;
                    curl_easy_getinfo(eh, CURLINFO_CONTENT_TYPE, &content_type);

                    if (content_type)
                    {
                        char *new_url = NULL;
                        curl_easy_getinfo(eh, CURLINFO_EFFECTIVE_URL, &new_url);

                        if (strcmp(content_type, "image/png") == 0)
                        {
                            printf("Content-Type is image/png. URL: %s\n", new_url);
                            if (is_png(recv_buf->buf))
                            {
                                png_count++;
                                url_list_add(&png_urls, new_url);
                                printf("Size of png url is: %d\n", png_urls.size);
                            }
                        }
                        if (strcmp(content_type, "text/html") == 0)
                        {

                            find_http(recv_buf->buf, recv_buf->size, 1, new_url, &frontier);
                        }
                        else
                        {
                            printf("Unknown content type: %s\n", content_type);
                        }
                    }

                    curl_multi_remove_handle(cm, eh);
                    curl_easy_cleanup(eh);
                    // free(recv_buf->buf);
                    // free(recv_buf);
                    
                    if (png_urls.size == max_pngs) {
                        while ((msg = curl_multi_info_read(cm, &msgs_left))) {
                            eh = msg->easy_handle;
                            curl_multi_remove_handle(cm, eh);
                            curl_easy_cleanup(eh);
                        }
                        break;
                    }
                }
            }
            else
            {
                fprintf(stderr, "error: after curl_multi_info_read(), CURLMsg=%d\n", msg->msg);
            }
        }
    }
    curl_multi_cleanup(cm);
    curl_global_cleanup();
    return;
}

int main(int argc, char **argv)
{
    char *logfile = NULL;
    xmlInitParser();

    int opt;
    while ((opt = getopt(argc, argv, "t:m:v:p:")) != -1)
    {
        if (opt == 't')
        {
            max_connections = atoi(optarg);
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
        max_pngs = 50;
        print_pngs = 50;
    }

    if (optind >= argc)
    {
        fprintf(stderr, "SEED URL not provided\n");
        return 1;
    }
    char *seed_url = argv[optind];

    url_list_init(&frontier);
    url_list_init(&visited);
    url_list_init(&png_urls);

    frontier.name = "frontier";
    visited.name = "visited";
    png_urls.name = "png_urls";

    url_list_add(&frontier, seed_url);

    struct timeval start, end;
    gettimeofday(&start, NULL);

    crawl();

    FILE *png_file = fopen("png_urls.txt", "w");
    if (png_file)
    {
        int limit = print_pngs < png_urls.size ? print_pngs : png_urls.size;
        // printf("png url size: %d\n", data.png_urls.size);
        // printf("print png size: %d\n", print_pngs);
        // printf("limit is: %d\n", limit);
        for (int i = 0; i < limit; i++)
        {
            fprintf(png_file, "%s\n", png_urls.data[i]);
        }
        fclose(png_file);
    }

    if (logfile)
    {
        FILE *log_file = fopen(logfile, "w");
        if (log_file)
        {
            for (int i = 0; i < visited.size; i++)
            {
                fprintf(log_file, "%s\n", visited.data[i]);
            }
            fclose(log_file);
        }
    }

    gettimeofday(&end, NULL);
    double time_taken = (end.tv_sec - start.tv_sec) * 1e6;
    time_taken = (time_taken + (end.tv_usec - start.tv_usec)) * 1e-6;
    printf("findpng3 execution time: %.6f seconds\n", time_taken);

    for (int i = 0; i < buffs_count; i++) {
        if (buffs[i] != NULL) {
            free(buffs[i]);
        }
    }
    for (int j = 0; j < data_count; j++) {
        if (buffs_data[j] != NULL) {
            free(buffs_data[j]);
        }
    }

    url_list_free(&frontier);
    url_list_free(&visited);
    url_list_free(&png_urls);
    xmlCleanupParser();

    free(logfile);

    return 0;
}
