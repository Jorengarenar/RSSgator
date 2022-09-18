#include "server.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>

#include <curl/curl.h>

#include "globals.h"
const char* BUFFER;

// cURL {{{1

struct MemoryStruct {
    char* memory;
    size_t size;
};

static size_t writeMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct* mem = (struct MemoryStruct*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

struct MemoryStruct getFeed(const char* feed)
{
    CURL* curl = curl_easy_init();
    if (!curl) {
        return (struct MemoryStruct){ NULL, 0 };
    }

    struct MemoryStruct chunk = { .memory = malloc(1), .size = 0 };

    curl_easy_setopt(curl, CURLOPT_URL, feed);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_OK) {
        printf("%lu bytes retrieved\n", (unsigned long)chunk.size);
    } else {
        fprintf(stderr,
                "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));
    }
    curl_easy_cleanup(curl);

    return chunk;
}


// libmicrohttpd {{{1

#define PORT            8888
#define POSTBUFFERSIZE  512
#define MAXURLSIZE      2000
#define MAXANSWERSIZE   4096

#define GET             0
#define POST            1

struct connection_info_struct {
    int connectiontype;
    char* answerstring;
    struct MHD_PostProcessor* postprocessor;
};

const char* askpage = "<html><body>"
                      "Get feed:<br>"
                      "<form action=\"/feed\" method=\"post\">"
                      "<input name=\"url\" type=\"text\">"
                      "<input type=\"submit\" value=\" Send \"></form>"
                      "</body></html>";

const char* greetingpage = "<html><body><h1>Welcome, %s!</center></h1></body></html>";

const char* errorpage = "<html><body>This doesn't seem to be right.</body></html>";


static enum MHD_Result send_page (struct MHD_Connection* connection, const char* page)
{
    struct MHD_Response* response = MHD_create_response_from_buffer(
        strlen (page),
        (void*)page,
        MHD_RESPMEM_PERSISTENT);
    if (!response) {
        return MHD_NO;
    }

    enum MHD_Result ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response (response);
    return ret;
}

static enum MHD_Result iterate_post(
    void* coninfo_cls, enum MHD_ValueKind kind, const char* key,
    const char* filename, const char* content_type,
    const char* transfer_encoding, const char* data, uint64_t off,
    size_t size)
{
    struct connection_info_struct* con_info = coninfo_cls;
    (void) kind;              // [unused]
    (void) filename;          // [unused]
    (void) content_type;      // [unused]
    (void) transfer_encoding; // [unused]
    (void) off;               // [unused]

    if (strcmp(key, "url") == 0) {
        if ((size > 0) && (size <= MAXURLSIZE)) {
            struct MemoryStruct chunk = getFeed(data);
            if (chunk.memory == NULL) {
                return MHD_NO;
            }

            /* char* answerstring = malloc(chunk.size); */
            /* if (!answerstring) { */
            /*     return MHD_NO; */
            /* } */

            con_info->answerstring = chunk.memory;
            /* free(chunk.memory); */
        } else {
            con_info->answerstring = NULL;
        }

        return MHD_NO;
    }

    return MHD_YES;
}

static void request_completed(void* cls, struct MHD_Connection* connection,
                              void** con_cls, enum MHD_RequestTerminationCode toe)
{
    struct connection_info_struct* con_info = *con_cls;
    (void) cls;        // [unused]
    (void) connection; // [unused]
    (void) toe;        // [unused]

    if (con_info == NULL) {
        return;
    }

    if (con_info->connectiontype == POST) {
        MHD_destroy_post_processor(con_info->postprocessor);
        if (con_info->answerstring) {
            free(con_info->answerstring);
        }
    }

    free(con_info);
    *con_cls = NULL;
}

static enum MHD_Result answer_to_connection(
    void* cls, struct MHD_Connection* connection,
    const char* url, const char* method,
    const char* version, const char* upload_data,
    size_t* upload_data_size, void** con_cls)
{
    (void) cls;     // [unused]
    (void) url;     // [unused]
    (void) version; // [unused]

    if (*con_cls == NULL) {
        struct connection_info_struct* con_info;

        con_info = malloc (sizeof (struct connection_info_struct));
        if (con_info == NULL) {
            return MHD_NO;
        }
        con_info->answerstring = NULL;

        if (strcmp(method, "POST") == 0) {
            con_info->postprocessor = MHD_create_post_processor(
                connection, POSTBUFFERSIZE,
                iterate_post, (void*) con_info);

            if (con_info->postprocessor == NULL) {
                free (con_info);
                return MHD_NO;
            }

            con_info->connectiontype = POST;
        } else {
            con_info->connectiontype = GET;
        }

        *con_cls = (void*)con_info;

        return MHD_YES;
    }

    if (strcmp(method, "GET") == 0) {
        return send_page(connection, askpage);
    }

    if (strcmp(method, "POST") == 0) {
        struct connection_info_struct* con_info = *con_cls;

        if (*upload_data_size != 0) {
            MHD_post_process(con_info->postprocessor,
                             upload_data,
                             *upload_data_size);
            *upload_data_size = 0;

            return MHD_YES;
        } else if (NULL != con_info->answerstring) {
            return send_page(connection, con_info->answerstring);
        }
    }


    /* struct MHD_Response* response = MHD_create_response_from_buffer( */
    /*     strlen(BUFFER), */
    /*     (void*)BUFFER, */
    /*     MHD_RESPMEM_PERSISTENT); */

    /* int ret = MHD_queue_response(connection, MHD_HTTP_OK, response); */
    /* /1* MHD_add_response_header(response, "Content-Type", "text/xml"); *1/ */
    /* MHD_destroy_response (response); */

    /* return ret; */

    return send_page(connection, errorpage);
}

struct MHD_Daemon* initServ()
{
    return MHD_start_daemon(
        MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
        PORT, NULL, NULL,
        &answer_to_connection, NULL,
        MHD_OPTION_NOTIFY_COMPLETED, request_completed,
        NULL, MHD_OPTION_END);
}
