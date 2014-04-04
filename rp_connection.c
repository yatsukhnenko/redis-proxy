#include "rp_event.h"
#include "rp_socket.h"
#include "rp_connection.h"

int rp_connection_handler_loop(rp_connection_t *l)
{
    int i;
    double ts;
    rp_event_t e;
    struct timeval tv;
    rp_server_t *server;
    rp_event_handler_t eh;
    rp_connection_pool_t *srv = l->data;
    rp_connection_t *c;

    if(rp_event_handler_init(&eh) == NULL) {
        return EXIT_FAILURE;
    }
    e.data = l;
    e.events = RP_EVENT_READ;
    if(eh.add(&eh, l->sockfd, &e) != RP_SUCCESS) {
        return EXIT_FAILURE;
    }
    for(;;) {
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        eh.poll(&eh, &tv);
        gettimeofday(&tv, NULL);
        ts = tv.tv_sec + tv.tv_usec / 1000000.0;
        for(i = 0; i < srv->size; i++) {
            c = &srv->c[i];
            if(c->sockfd < 0) {
                if(c->time + 1 < ts) {
                    c->time = ts;
                    if(rp_server_connect(c) != NULL) {
                        e.data = c;
                        e.events = RP_EVENT_WRITE;
                        if(eh.add(&eh, c->sockfd, &e) != RP_SUCCESS) {
                            c->on.close(c, &eh);
                        }
                    }
                }
            } else {
                server = c->data;
                if(c->time + c->settings.ping < ts
                    && !(c->flags & RP_MAINTENANCE) && !server->queue.size) {
                    e.data = c;
                    e.events = RP_EVENT_WRITE;
                    if(eh.add(&eh, c->sockfd, &e) != RP_SUCCESS) {
                        continue;
                    }
                    c->flags |= RP_MAINTENANCE;
                    c->time = ts;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

int rp_connection_accept(rp_connection_t *l, void *data)
{
    rp_event_t e;
    rp_connection_t *c;
    rp_event_handler_t *eh = data;

    if((c = rp_client_accept(l)) != NULL) {
        e.data = c;
        e.events = RP_EVENT_READ;
        if(eh->add(eh, c->sockfd, &e) != RP_SUCCESS) {
            c->on.close(c, eh);
        }
    }
    return RP_SUCCESS;
}

int rp_server_connection_read(rp_connection_t *c, void *data)
{
    int rv;
    rp_event_t e;
    rp_string_t s;
    rp_client_t *client;
    rp_server_t *server = c->data;
    rp_event_handler_t *eh = data;
    rp_connection_t *l = c->connection;

    if(c->flags & RP_MAINTENANCE) {
        if(rp_recv(c->sockfd, &server->buffer) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        if((rv = rp_reply_parse(&server->buffer, &server->cmd)) == RP_UNKNOWN) {
            return RP_SUCCESS;
        }
        server->buffer.used = 0;
        if(l->flags & RP_MAINTENANCE) {
            l->flags &= ~RP_ALREADY;
            e.data = c;
            e.events = RP_EVENT_READ;
            if(eh->del(eh, c->sockfd, &e) != RP_SUCCESS) {
                return RP_FAILURE;
            }
            e.events = RP_EVENT_WRITE;
            if(eh->add(eh, c->sockfd, &e) != RP_SUCCESS) {
                return RP_FAILURE;
            }
            if(rv == RP_SUCCESS && *server->buffer.s.data == RP_BULK_PREFIX) {
                if((server->master = rp_replication_info_parse(&server->cmd.argv, l->data)) == NULL) {
                    l->connection = c;
                } else {
                    l->connection = server->master;
                    rp_set_slaveof(c, l->connection);
                }
                rp_set_slaveof(l->connection, NULL);
                l->flags &= ~RP_MAINTENANCE;
            }
        } else {
            if(*server->buffer.s.data == RP_STATUS_PREFIX) {
                c->flags &= ~RP_MAINTENANCE;
            } else {
                e.data = c;
                e.events = RP_EVENT_READ;
                if(eh->del(eh, c->sockfd, &e) != RP_SUCCESS) {
                    return RP_FAILURE;
                }
                e.events = RP_EVENT_WRITE;
                if(eh->add(eh, c->sockfd, &e) != RP_SUCCESS) {
                    return RP_FAILURE;
                }
            }
        }
    } else {
        if(rp_recv(c->sockfd, &server->in) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        while(server->in.r < server->in.used) {
            if(server->client == NULL) {
                if((server->client = rp_queue_shift(&server->queue)) == NULL) {
                    return RP_FAILURE;
                }
                e.data = server->client;
                e.events = RP_EVENT_WRITE;
                if(eh->add(eh, server->client->sockfd, &e) != RP_SUCCESS) {
                    return RP_FAILURE;
                }
            }
            client = server->client->data;
            s.data = &server->in.s.data[server->in.r];
            rv = rp_reply_parse(&server->in, &client->cmd);
            s.length = &server->in.s.data[server->in.r] - s.data;
            if(rp_buffer_append(&client->buffer, &s) != RP_SUCCESS) {
                server->client->on.close(server->client, eh);
                server->client = NULL;
                continue;
            }
            if(rv != RP_UNKNOWN) {
                client->server = NULL;
                server->client->flags |= RP_ALREADY;
                server->client = NULL;
                continue;
            }
            break;
        }
        if(server->in.r == (int)server->in.used) {
            server->in.r = server->in.w = 0;
            server->in.used = 0;
        }
    }
    return RP_SUCCESS;
}

int rp_server_connection_write(rp_connection_t *c, void *data)
{
    rp_event_t e;
    rp_event_handler_t *eh = data;
    rp_connection_t *l = c->connection;
    rp_server_t *server = c->data;

    if(!(c->flags & RP_ESTABLISHED)) {
        if(rp_server_connect(c) == NULL) {
            return RP_FAILURE;
        }
        return RP_SUCCESS;
    }
    if(c->flags & RP_MAINTENANCE) {
        if(l->connection == NULL) {
            if(l->flags & RP_MAINTENANCE) {
                if(l->flags & RP_ALREADY) {
                    return RP_SUCCESS;
                }
                l->flags |= RP_ALREADY;
                server->buffer.used = sprintf(server->buffer.s.data,
                    "*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n");
            } else {
                l->connection = c;
                rp_set_slaveof(c, NULL);
                syslog(LOG_INFO, "Set %s:%d slave of no one",
                    c->settings.address.data, c->settings.port);
            }
        } else if(c != l->connection && server->master != l->connection) {
            if(l->connection->flags & RP_MAINTENANCE) {
                return RP_SUCCESS;
            }
            rp_set_slaveof(c, l->connection);
            syslog(LOG_INFO, "Set %s:%d slave of %s:%d",
                c->settings.address.data, c->settings.port,
                l->connection->settings.address.data,
                l->connection->settings.port);
        } else if(!server->buffer.used) {
            server->buffer.used = sprintf(server->buffer.s.data, "*1\r\n$4\r\nPING\r\n");
            server->buffer.r = server->buffer.w = 0;
        }
        if(rp_send(c->sockfd, &server->buffer) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        if(server->buffer.w == server->buffer.used) {
            e.data = c;
            e.events = RP_EVENT_WRITE;
            if(eh->del(eh, c->sockfd, &e) != RP_SUCCESS) {
                return RP_FAILURE;
            }
            e.events = RP_EVENT_READ;
            if(eh->add(eh, c->sockfd, &e) != RP_SUCCESS) {
                return RP_FAILURE;
            }
            server->cmd.i = RP_NULL_STRLEN;
            server->cmd.argc = RP_NULL_STRLEN - 1;
            server->buffer.r = server->buffer.w = 0;
            server->buffer.used = 0;
        }
    } else {
        if(rp_send(c->sockfd, &server->out) != RP_SUCCESS) {
            return RP_SUCCESS;
        }
        if(server->out.w == server->out.used) {
            e.data = c;
            e.events = RP_EVENT_WRITE;
            if(eh->del(eh, c->sockfd, &e) == RP_SUCCESS) {
                server->out.r = server->out.w = 0;
                server->out.used = 0;
            }
        }
    }
    return RP_SUCCESS;
}

void rp_server_connection_close(rp_connection_t *c, void *data)
{
    int i;
    rp_event_t e;
    struct timeval tv;
    rp_client_t *client;
    rp_server_t *server;
    rp_connection_t *l = c->connection;
    rp_connection_pool_t *srv = l->data;
    rp_event_handler_t *eh = data;

    if(c == l->connection) {
        l->connection = NULL;
        for(i = 0; i < srv->size; i++) {
            srv->c[i].time = 0;
        }
    }
    server = c->data;
    while((server->client = rp_queue_shift(&server->queue)) != NULL) {
        client = server->client->data;
        client->server = NULL;
        server->client->on.close(server->client, eh);
    }
    if(c->flags & RP_ESTABLISHED) {
        syslog(LOG_ERR, "Close connection to %s:%d",
            c->settings.address.data, c->settings.port);
    }
    e.events = RP_EVENT_READ | RP_EVENT_WRITE;
    eh->del(eh, c->sockfd, &e);
    eh->events[c->sockfd].events = 0;
    eh->events[c->sockfd].data = NULL;
    gettimeofday(&tv, NULL);
    c->time = tv.tv_sec + tv.tv_usec / 1000000.0;
    server->master = NULL;
    server->buffer.used = 0;
    server->buffer.r = server->buffer.w = 0;
    server->in.used = server->out.used = 0;
    server->in.r = server->out.w = 0;
    c->flags = RP_SERVER;
    close(c->sockfd);
    c->sockfd = -1;
}

int rp_client_connection_read(rp_connection_t *c, void *data)
{
    int i;
    rp_event_t e;
    rp_string_t s, *sp;
    rp_server_t *server;
    rp_client_t *client = c->data;
    rp_connection_t *l = c->connection;
    rp_event_handler_t *eh = data;

    if(rp_recv(c->sockfd, &client->buffer) != RP_SUCCESS) {
        return RP_FAILURE;
    }
    if((i = rp_request_parse(&client->buffer, &client->cmd)) != RP_UNKNOWN) {
        e.data = c;
        e.events = RP_EVENT_READ;
        if(eh->del(eh, c->sockfd, &e) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        sp = NULL;
        s.data = NULL;
        s.length = RP_NULL_STRLEN;
        if(i != RP_SUCCESS) {
            sp = rp_sprintf(&s, "-ERR syntax error");
        } else if((client->cmd.proto = rp_command_lookup(&client->cmd.name)) == NULL) {
            sp = rp_sprintf(&s, "-ERR unknown command '%s'", &client->cmd.name);
        } else if(!(c->flags & RP_AUTHENTICATED) && !(client->cmd.proto->flags & RP_WITHOUT_AUTH)) {
            sp = rp_sprintf(&s, "-ERR operation not permitted");
        } else if((client->cmd.proto->argc < 0 && client->cmd.argc < abs(client->cmd.proto->argc))
            || (client->cmd.proto->argc > 0 && client->cmd.argc != client->cmd.proto->argc)) {
            sp = rp_sprintf(&s, "-ERR wrong number of arguments for '%s' command", &client->cmd.name);
        } else {
            sp = (client->cmd.proto->flags & RP_LOCAL_COMMAND) ? client->cmd.proto->handler(&s, c) : &s;
        }
        if(sp == NULL) {
            return RP_FAILURE;
        } else if(sp->data != NULL) {
            e.events = RP_EVENT_WRITE;
            if(eh->add(eh, c->sockfd, &e) != RP_SUCCESS) {
                free(sp->data);
                return RP_FAILURE;
            }
            client->buffer.used = sprintf(client->buffer.s.data, "%.*s\r\n", sp->length, sp->data);
            client->buffer.r = client->buffer.w = 0;
            c->flags |= RP_ALREADY;
            free(sp->data);
            return RP_SUCCESS;
        }
        client->server = (client->cmd.proto->flags & RP_MASTER_COMMAND)
            ? l->connection
            : rp_server_lookup(l->data);
        if(client->server == NULL) {
            return RP_FAILURE;
        }
        e.data = client->server;
        e.events = RP_EVENT_WRITE;
        if(eh->add(eh, client->server->sockfd, &e) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        server = client->server->data;
        if(rp_buffer_append(&server->out, &client->cmd.txt) != RP_SUCCESS) {
            client->server->time = 0;
            return RP_FAILURE;
        } else if(rp_queue_push(&server->queue, c) != RP_SUCCESS) {
            server->out.used -= client->cmd.txt.length;
            client->server->time = 0;
            return RP_FAILURE;
        }
        c->flags &= ~RP_ALREADY;
        client->cmd.i = RP_NULL_STRLEN;
        client->cmd.argc = RP_NULL_STRLEN - 1;
        client->buffer.r = client->buffer.w = 0;
        client->buffer.used = 0;
    }
    return RP_SUCCESS;
}

int rp_client_connection_write(rp_connection_t *c, void *data)
{
    rp_event_t e;
    rp_client_t *client = c->data;
    rp_event_handler_t *eh = data;

    if(rp_send(c->sockfd, &client->buffer) != RP_SUCCESS) {
        return RP_FAILURE;
    }
    if(client->buffer.w == client->buffer.used && c->flags & RP_ALREADY) {
        if(c->flags & RP_SHUTDOWN) {
            return RP_FAILURE;
        }
        e.data = c;
        e.events = RP_EVENT_WRITE;
        if(eh->del(eh, c->sockfd, &e) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        e.events = RP_EVENT_READ;
        if(eh->add(eh, c->sockfd, &e) != RP_SUCCESS) {
            return RP_FAILURE;
        }
        client->buffer.used = 0;
        client->buffer.r = client->buffer.w = 0;
        client->cmd.argc = RP_NULL_STRLEN - 1;
        c->flags &= ~RP_ALREADY;
    }
    return RP_SUCCESS;
}

void rp_client_connection_close(rp_connection_t *c, void *data)
{
    rp_event_t e;
    rp_client_t *client = c->data;
    rp_event_handler_t *eh = data;

    e.events = RP_EVENT_READ | RP_EVENT_WRITE;
    eh->del(eh, c->sockfd, &e);
    eh->events[c->sockfd].events = 0;
    eh->events[c->sockfd].data = NULL;
    close(c->sockfd);
    free(client->buffer.s.data);
    free(c->data);
    free(c);
}

rp_connection_t *rp_server_connect(rp_connection_t *c)
{
    int optval = 0;
    struct sockaddr_in addr;
    rp_server_t *server = c->data;
    socklen_t optlen = sizeof(optval);

    if(!(c->flags & RP_SERVER)) {
        if(server == NULL) {
            /* initialize server structure */
            if((server = malloc(sizeof(rp_server_t))) == NULL) {
                syslog(LOG_ERR, "malloc at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
                return NULL;
            }
            memset(server, 0, sizeof(rp_server_t));
            c->data = server;
        }
        if(server->buffer.s.data == NULL) {
            /* initialize buffer */
            server->buffer.s.length = RP_BUFFER_SIZE;
            if((server->buffer.s.data = malloc(server->buffer.s.length)) == NULL) {
                server->buffer.s.length = RP_NULL_STRLEN;
                return NULL;
            }
            server->buffer.r = server->buffer.w = 0;
            server->buffer.used = 0;
        }
        server->client = NULL;
        server->master = NULL;
        server->queue.size = 0;
        server->queue.first = server->queue.last = NULL;
        server->in.s.data = server->out.s.data = NULL;
        server->in.s.length = server->out.s.length = RP_NULL_STRLEN;
        server->in.used = server->out.used = 0;
        server->in.r = server->out.w = 0;
        c->on.read = rp_server_connection_read;
        c->on.write = rp_server_connection_write;
        c->on.close = rp_server_connection_close;
        c->flags |= RP_SERVER;
    }

    if(c->sockfd < 0) {
        /* create socket and make it nonblocking */
        if(rp_socket_init(&c->sockfd) == NULL) {
            c->sockfd = -1;
            return NULL;
        }
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = c->address;
        addr.sin_port = c->port;
        /* try to establish connection to server */
        if(connect(c->sockfd, (const struct sockaddr *)&addr, sizeof(addr))) {
            if(errno != EINPROGRESS) {
                syslog(LOG_NOTICE, "Could not connect to Redis at %s:%d: %s",
                    c->settings.address.data, c->settings.port, strerror(errno));
                close(c->sockfd);
                c->sockfd = -1;
                return NULL;
            }
        } else {
            /* connection has been established successfully */
            c->flags |= RP_ESTABLISHED;
            syslog(LOG_NOTICE, "Successfully connected to %s:%d",
                c->settings.address.data, c->settings.port);
        }
    } else if(!(c->flags & RP_ESTABLISHED)) {
        /* check previous connection attempt state */
        if(getsockopt(c->sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
            syslog(LOG_ERR, "getsockopt at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        } else if(optval) {
            if(optval != EALREADY) {
                syslog(LOG_NOTICE, "Could not connect to Redis at %s:%d: %s",
                    c->settings.address.data, c->settings.port, strerror(optval));
                close(c->sockfd);
                c->sockfd = -1;
                return NULL;
            }
        } else {
            /* connection has been established successfully */
            c->flags |= RP_ESTABLISHED;
            syslog(LOG_NOTICE, "Successfully connected to %s:%d",
                c->settings.address.data, c->settings.port);
        }
    }
    c->flags |= RP_MAINTENANCE;
    return c;
}

rp_connection_t *rp_client_accept(rp_connection_t *l)
{
    int sockfd;
    rp_connection_t *c;
    rp_client_t *client;
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    if((sockfd = accept(l->sockfd, (struct sockaddr *)&addr, &len)) < 0) {
        syslog(LOG_ERR, "accept at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    if((c = malloc(sizeof(rp_connection_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(sockfd);
        return NULL;
    }
    if((client = malloc(sizeof(rp_client_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(sockfd);
        free(c);
        return NULL;
    }
    memset(client, 0, sizeof(rp_client_t));
    client->buffer.s.length = RP_BUFFER_SIZE;
    if((client->buffer.s.data = malloc(client->buffer.s.length)) == NULL) {
        client->buffer.s.length = RP_NULL_STRLEN;
    }
    client->cmd.argc = RP_NULL_STRLEN - 1;
    client->server = NULL;
    c->on.read = rp_client_connection_read;
    c->on.write = rp_client_connection_write;
    c->on.close = rp_client_connection_close;
    c->connection = l;
    c->data = client;
    c->sockfd = sockfd;
    c->flags = RP_CLIENT;
    c->settings.auth.data = l->settings.auth.data;
    c->settings.auth.length = l->settings.auth.length;
    if(l->settings.auth.data == NULL) {
        c->flags |= RP_AUTHENTICATED;
    }
    return c;
}

void rp_set_slaveof(rp_connection_t *c, rp_connection_t *m)
{
    char b[6];
    rp_server_t *server = c->data;

    if(m == NULL) {
        server->buffer.used = sprintf(server->buffer.s.data,
            "*3\r\n$7\r\nSLAVEOF\r\n$2\r\nNO\r\n$3\r\nONE\r\n");
        m = c;
    } else {
        server->buffer.used = sprintf(server->buffer.s.data,
            "*3\r\n$7\r\nSLAVEOF\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n",
            m->settings.address.length, m->settings.address.data,
            sprintf(b, "%u", m->settings.port), b);
    }
    c->flags |= RP_MAINTENANCE;
    server->buffer.r = server->buffer.w = 0;
    server->master = m;
}

rp_connection_t *rp_replication_info_parse(rp_string_t *info, rp_connection_pool_t *srv)
{
    int i;
    rp_string_t s;
    char *ptr, *val;
    in_addr_t address = INADDR_NONE;
    in_port_t port = 0;

    s.data = info->data;
    s.length = info->length;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        *ptr = '\0';
        if((val = strchr(s.data, ':')) != NULL) {
            *val++ = '\0';
            if(!strcmp(s.data, "role")) {
                if(!strcmp(val, "master")) {
                    return NULL;
                }
            } else if(!strcmp(s.data, "master_host")) {
                address = inet_addr(val);
            } else if(!strcmp(s.data, "master_port")) {
                port = htons(strtol(val, NULL, 10));
            } else if(!strcmp(s.data, "master_link_status")) {
                if(!strcmp(val, "down")) {
                    return NULL;
                }
            }
        }
        ptr += 2;
        s.length -= ptr - s.data;
        s.data = ptr;
    }
    for(i = 0; i < srv->size; i++) {
        if(srv->c[i].address == address && srv->c[i].port == port) {
            return &srv->c[i];
        }
    }
    return NULL;
}

rp_connection_t *rp_server_lookup(rp_connection_pool_t *s)
{
    int i;

    for(i = 0; i < s->size; i++) {
        if(s->i == s->size) {
            s->i = 0;
        }
        if(s->c[s->i].sockfd >= 0 && s->c[s->i].flags & RP_ESTABLISHED) {
            return &s->c[s->i++];
        }
        s->i++;
    }
    return NULL;
}
