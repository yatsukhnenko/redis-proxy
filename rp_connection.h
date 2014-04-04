#ifndef _RP_CONNECTION_H_
#define _RP_CONNECTION_H_

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "rp_common.h"
#include "rp_redis.h"
#include "rp_queue.h"


#define RP_SERVER        0x1000
#define RP_CLIENT        0x2000

#define RP_ALREADY       0x0100
#define RP_SHUTDOWN      0x0200

#define RP_ESTABLISHED   0x0010
#define RP_MAINTENANCE   0x0020
#define RP_AUTHENTICATED 0x0040

typedef struct rp_connection {
    int sockfd;
    double time;
    unsigned short flags;
    in_addr_t address;
    in_port_t port;
    struct {
        time_t ping;
        rp_string_t auth;
        rp_string_t address;
        unsigned short port;
    } settings;
    struct rp_connection *connection;
    void *data;
    struct {
        int (*read)(struct rp_connection *c, void *data);
        int (*write)(struct rp_connection *c, void *data);
        void (*close)(struct rp_connection *c, void *data);
    } on;
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
    rp_queue_t queue;
    rp_command_t cmd;
    rp_buffer_t buffer;
    rp_buffer_t in, out;
    rp_connection_t *client;
    rp_connection_t *master;
} rp_server_t;


int rp_connection_handler_loop(rp_connection_t *l);
int rp_connection_accept(rp_connection_t *l, void *data);
int rp_server_connection_read(rp_connection_t *c, void *data);
int rp_server_maintenance_read(rp_connection_t *c, void *data);
int rp_client_connection_read(rp_connection_t *c, void *data);
int rp_server_connection_write(rp_connection_t *c, void *data);
int rp_server_maintenance_write(rp_connection_t *c, void *data);
int rp_client_connection_write(rp_connection_t *c, void *data);
void rp_server_connection_close(rp_connection_t *c, void *data);
void rp_client_connection_close(rp_connection_t *c, void *data);
rp_connection_t *rp_client_accept(rp_connection_t *l);
rp_connection_t *rp_server_connect(rp_connection_t *c);
rp_connection_t *rp_server_lookup(rp_connection_pool_t *srv);
void rp_set_slaveof(rp_connection_t *c, rp_connection_t *m);
rp_connection_t *rp_replication_info_parse(rp_string_t *info, rp_connection_pool_t *srv);

#endif /* _RP_CONNECTION_H_ */
