#ifndef _RP_COMMON_H_
#define _RP_COMMON_H_

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

#include "rp_auto.h"
#include "rp_string.h"

#define RP_FAILURE -1
#define RP_UNKNOWN  0
#define RP_SUCCESS  1

#define RP_TIMEOUT                1

#define RP_BUFFER_SIZE            4096
#define RP_CONCURRENT_CONNECTIONS 1024

#define RP_DEFAULT_PORT           6379

typedef struct {
    size_t used;
    rp_string_t s;
    unsigned int r;
    unsigned int w;
} rp_buffer_t;

int rp_buffer_resize(rp_buffer_t *buffer, int delta);

#endif /* _RP_COMMON_H_ */
