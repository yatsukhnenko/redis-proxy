#ifndef _RP_REDIS_H_
#define _RP_REDIS_H_

#include <strings.h>
#include "rp_common.h"

#define RP_NULL_LEN          -1

#define RP_LOCAL_COMMAND      1
#define RP_MASTER_COMMAND     2
#define RP_SLAVE_COMMAND      4

#define RP_MULTI_BULK_PREFIX '*'
#define RP_BULK_PREFIX       '$'
#define RP_STATUS_PREFIX     '+'
#define RP_ERROR_PREFIX      '-'
#define RP_INTEGER_PREFIX    ':'

struct rp_command_proto {
    char *name;
    unsigned int flags;
    int argc;
    void (*handler)(void *data);
};

typedef struct {
    int argc;
    struct {
        char *ptr;
        int length;
    } argv;
    unsigned int i;
    struct rp_command_proto *proto;
} rp_command_t;

struct rp_command_proto *rp_lookup_command(char *name, int length);
void rp_command_ping(void *data);
void rp_command_quit(void *data);
void rp_command_time(void *data);
void rp_command_error(void *data);

#endif /* _RP_REDIS_H_ */
