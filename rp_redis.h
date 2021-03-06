#ifndef _RP_REDIS_H_
#define _RP_REDIS_H_

#include <strings.h>
#include "rp_common.h"

#define RP_COMMAND_NAME_MAX   16

#define RP_LOCAL_COMMAND      0x01
#define RP_MASTER_COMMAND     0x02
#define RP_SLAVE_COMMAND      0x04
#define RP_WITHOUT_AUTH       0x10

#define RP_MULTI_BULK_PREFIX '*'
#define RP_BULK_PREFIX       '$'
#define RP_STATUS_PREFIX     '+'
#define RP_ERROR_PREFIX      '-'
#define RP_INTEGER_PREFIX    ':'

typedef struct {
    char *name;
    unsigned int flags;
    int argc;
    rp_string_t *(*handler)(rp_string_t *s, void *data);
} rp_command_proto_t;

typedef struct {
    int argc;
    unsigned int i;
    rp_string_t argv;
    rp_string_t name;
    rp_command_proto_t *proto;
    rp_string_t txt;
} rp_command_t;

rp_command_proto_t *rp_command_lookup(rp_string_t *name);
rp_string_t *rp_command_auth(rp_string_t *s, void *data);
rp_string_t *rp_command_ping(rp_string_t *s, void *data);
rp_string_t *rp_command_quit(rp_string_t *s, void *data);
int rp_request_parse(rp_buffer_t *buffer, rp_command_t *cmd);
int rp_reply_parse(rp_buffer_t *buffer, rp_command_t *cmd);

#endif /* _RP_REDIS_H_ */
