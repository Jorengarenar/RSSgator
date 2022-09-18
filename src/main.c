#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <fcntl.h>

#include <curl/curl.h>
#include <sqlite3.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlerror.h>
#include <libxml/xpath.h>

#include "server.h"
#include "globals.h"

#define FEED "https://blog.joren.ga/feed.xml"

sqlite3* initDb()
{
    sqlite3* db;
    int rc = sqlite3_open(".rssgator.db", &db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return NULL;
    }

    /* char* zErrMsg = 0; */
    /* rc = sqlite3_exec(db, argv[2], callback, 0, &zErrMsg); */
    /* if (rc != SQLITE_OK) { */
    /*     fprintf(stderr, "SQL error: %s\n", zErrMsg); */
    /*     sqlite3_free(zErrMsg); */
    /* } */

    return db;
}

int main()
{
    sqlite3* db = initDb();
    curl_global_init(CURL_GLOBAL_DEFAULT);
    struct MHD_Daemon* daemon = initServ();
    if (daemon == NULL) {
        return 1;
    }
    getchar();

    curl_global_cleanup();
    MHD_stop_daemon(daemon);
    sqlite3_close(db);

    return 0;
}
