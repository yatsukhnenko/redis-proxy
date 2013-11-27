#ifndef _RP_CONNECTION_H_
#define _RP_CONNECTION_H_

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rp_common.h"
#include "rp_event.h"
#include "rp_redis.h"
#include "rp_queue.h"


#define RP_SERVER        0x10000000
#define RP_CLIENT        0x20000000
#define RP_MASTER        0x40000000

#define RP_ALREADY       0x01000000
#define RP_INPROGRESS    0x02000000
#define RP_SHUTDOWN      0x04000000

#define RP_ESTABLISHED   0x00100000
#define RP_MAINTENANCE   0x00200000
#define RP_AUTHENTICATED 0x00400000

typedef struct {
    int sockfd;
    time_t time;
    unsigned int flags;
    rp_string_t auth;
    struct {
        rp_string_t address;
        unsigned short port;
    } hr;
    in_addr_t address;
    in_port_t port;
    void *data;
} rp_connection_t;

typedef struct {
    size_t size;
    unsigned int i;
    rp_connection_t *c;
} rp_connection_pool_t;

typedef struct {
    rp_buffer_t buffer;
    rp_command_t cmd;
    rp_connection_t *server;
} rp_client_t;

typedef struct {
    rp_buffer_t buffer;
    rp_connection_t *client;
    rp_connection_t *master;
} rp_server_t;


int rp_connection_handler_loop(rp_connection_t *l, rp_connection_pool_t *s);
rp_connection_t *rp_connection_accept(rp_connection_t *l);
rp_connection_t *rp_server_connect(rp_connection_t *c);
void rp_connection_close(rp_connection_t *c, rp_event_handler_t *eh, rp_connection_pool_t *s);
rp_connection_t *rp_server_lookup(rp_connection_pool_t *s);
void rp_set_slaveof(rp_connection_t *c, rp_connection_t *m);
int rp_request_parse(rp_buffer_t *buf, rp_command_t *cmd);
int rp_reply_parse(rp_buffer_t *buf, rp_command_t *cmd);

#endif /* _RP_CONNECTION_H_ */
