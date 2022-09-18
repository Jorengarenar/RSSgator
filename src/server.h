#ifndef SERVER_H_
#define SERVER_H_

#include <microhttpd.h>

struct MHD_Daemon;

struct MHD_Daemon* initServ();

#endif // SERVER_H_
