#include "rp_socket.h"
#include "rp_connection.h"

static rp_connection_t *master = NULL;

int rp_connection_handler_loop(rp_connection_t *l, rp_connection_pool_t *srv)
{
    time_t t;
    int i, rv;
    rp_event_t e;
    rp_string_t s, *sp;
    rp_client_t *client;
    rp_server_t *server;
    struct timeval timeout;
    rp_event_handler_t eh;
    rp_connection_t *c;

    /* initialize event handler */
    if(rp_event_handler_init(&eh, RP_CONCURRENT_CONNECTIONS) == NULL) {
        return EXIT_FAILURE;
    }
    /* handle events on listen socket */
    e.data = l;
    e.events = RP_EVENT_READ;
    if(eh.add(&eh, l->sockfd, &e) != RP_SUCCESS) {
        return EXIT_FAILURE;
    }

    /* main loop */
    for(;;) {
        time(&t);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        for(i = 0; i < eh.wait(&eh, &timeout); i++) {
            c = eh.ready[i].data;
            if(c->flags & RP_CLIENT) {
                client = c->data;
                if(eh.ready[i].events & RP_EVENT_READ) {
                    if(rp_recv(c->sockfd, &client->buffer) != RP_SUCCESS) {
                        rp_connection_close(c, &eh, srv);
                        continue;
                    }
                    if((rv = rp_request_parse(&client->buffer, &client->cmd)) != RP_UNKNOWN) {
                        e.data = c;
                        e.events = RP_EVENT_READ;
                        eh.del(&eh, c->sockfd, &e);
                        sp = NULL;
                        s.data = NULL;
                        s.length = RP_NULL_STRLEN;
                        if(rv != RP_SUCCESS) {
                            sp = rp_sprintf(&s, "-ERR syntax error");
                        } else if((client->cmd.proto = rp_command_lookup(&client->cmd.name)) == NULL) {
                            sp = rp_sprintf(&s, "-ERR unknown command '%s'", &client->cmd.name);
                        } else if(!(c->flags & RP_AUTHENTICATED) && !(client->cmd.proto->flags & RP_WITHOUT_AUTH)) {
                            sp = rp_sprintf(&s, "-ERR operation not permitted");
                        } else if((client->cmd.proto->argc < 0 && client->cmd.argc < abs(client->cmd.proto->argc))
                            || (client->cmd.proto->argc > 0 && client->cmd.argc != client->cmd.proto->argc)) {
                            sp = rp_sprintf(&s, "-ERR wrong number of arguments for '%s' command", &client->cmd.name);
                        } else {
                            sp = client->cmd.proto->flags & RP_LOCAL_COMMAND ? client->cmd.proto->handler(&s, c) : &s;
                        }
                        if(sp == NULL) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        } else if(sp->data != NULL) {
                            e.events = RP_EVENT_WRITE;
                            eh.add(&eh, c->sockfd, &e);
                            client->buffer.r = client->buffer.w = 0;
                            client->buffer.used = sprintf(client->buffer.s.data, "%.*s\r\n", sp->length, sp->data);
                            c->flags |= RP_ALREADY;
                            free(sp->data);
                            continue;
                        }
                        client->server = client->cmd.proto->flags & RP_MASTER_COMMAND
                            ? master : rp_server_lookup(srv);
                        if(client->server == NULL) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        server = client->server->data;
                        if(rp_buffer_append(&server->out, &client->cmd.txt) != RP_SUCCESS) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        } else if(rp_queue_push(&server->queue, c) != RP_SUCCESS) {
                            server->out.used -= client->cmd.txt.length;
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        c->flags &= ~RP_ALREADY;
                        client->cmd.i = RP_NULL_STRLEN;
                        client->cmd.argc = RP_NULL_STRLEN - 1;
                        client->buffer.r = client->buffer.w = 0;
                        client->buffer.used = 0;
                        e.data = client->server;
                        e.events = RP_EVENT_WRITE;
                        eh.add(&eh, client->server->sockfd, &e);
                    }
                }
                if(eh.ready[i].events & RP_EVENT_WRITE) {
                    if(rp_send(c->sockfd, &client->buffer) != RP_SUCCESS) {
                        rp_connection_close(c, &eh, srv);
                        continue;
                    }
                    if(client->buffer.w == client->buffer.used && c->flags & RP_ALREADY) {
                        if(c->flags & RP_SHUTDOWN) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        e.data = c;
                        e.events = RP_EVENT_WRITE;
                        eh.del(&eh, c->sockfd, &e);
                        client->buffer.r = client->buffer.w = 0;
                        client->buffer.used = 0;
                        client->cmd.i = RP_NULL_STRLEN;
                        client->cmd.argc = RP_NULL_STRLEN - 1;
                        e.events = RP_EVENT_READ;
                        eh.add(&eh, c->sockfd, &e);
                        c->flags &= ~RP_ALREADY;
                    }
                }
            } else if(c->flags & RP_SERVER) {
                c->time = t;
                server = c->data;
                if(eh.ready[i].events & RP_EVENT_READ) {
                    if(c->flags & RP_MAINTENANCE) {
                        if(rp_recv(c->sockfd, &server->buffer) != RP_SUCCESS) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        if(*server->buffer.s.data == RP_STATUS_PREFIX) {
                            c->flags &= ~RP_MAINTENANCE;
                            if(server->queue.size) {
                                e.data = c;
                                e.events = RP_EVENT_WRITE;
                                eh.add(&eh, c->sockfd, &e);
                            }
                        }
                    } else {
                        if(rp_recv(c->sockfd, &server->in) != RP_SUCCESS) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        while(server->in.r < server->in.used) {
                            if(server->client == NULL) {
                                server->client = rp_queue_shift(&server->queue);
                                e.data = server->client;
                                e.events = RP_EVENT_WRITE;
                                eh.add(&eh, server->client->sockfd, &e);
                            }
                            client = server->client->data;
                            s.data = &server->in.s.data[server->in.r];
                            rv = rp_reply_parse(&server->in, &client->cmd);
                            s.length = &server->in.s.data[server->in.r] - s.data;
                            if(rp_buffer_append(&client->buffer, &s) != RP_SUCCESS) {
                                rp_connection_close(server->client, &eh, srv);
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
                }
                if(eh.ready[i].events & RP_EVENT_WRITE) {
                    if(!(c->flags & RP_ESTABLISHED)) {
                        /* check previous connection attempt */
                        if(rp_server_connect(c) == NULL) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                    }
                    if(c->flags & RP_MAINTENANCE) {
                        if(master == NULL) {
                            master = c;
                            rp_set_slaveof(c, NULL);
                            syslog(LOG_INFO, "Set %s:%d slave of no one",
                                c->settings.address.data, c->settings.port);
                        } else if(!(c->flags & RP_MASTER) && server->master != master) {
                            if(master->flags & RP_MAINTENANCE) {
                                /* master is not ready */
                                continue;
                            }
                            rp_set_slaveof(c, master);
                            syslog(LOG_INFO, "Set %s:%d slave of %s:%d",
                                c->settings.address.data, c->settings.port,
                                master->settings.address.data, master->settings.port);
                        } else if(!server->buffer.used) {
                            server->buffer.used = sprintf(server->buffer.s.data, "*1\r\n$4\r\nPING\r\n");
                            server->buffer.r = server->buffer.w = 0;
                        }
                        /* send request to server */
                        if(rp_send(c->sockfd, &server->buffer) != RP_SUCCESS) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        if(server->buffer.w == server->buffer.used) {
                            /* all data transmitted to server */
                            e.data = c;
                            e.events = RP_EVENT_WRITE;
                            eh.del(&eh, c->sockfd, &e);
                            server->buffer.r = server->buffer.w = 0;
                            server->buffer.used = 0;
                        }
                    } else {
                        if(rp_send(c->sockfd, &server->out) != RP_SUCCESS) {
                            rp_connection_close(c, &eh, srv);
                            continue;
                        }
                        if(server->out.w == server->out.used) {
                            e.data = c;
                            e.events = RP_EVENT_WRITE;
                            eh.del(&eh, c->sockfd, &e);
                            server->out.r = server->out.w = 0;
                            server->out.used = 0;
                        }
                    }
                }
            } else {
                /* accept connection from client */
                if((c = rp_connection_accept(l)) != NULL) {
                    e.data = c;
                    e.events = RP_EVENT_READ;
                    eh.add(&eh, c->sockfd, &e);
                }
            }
        }
        for(i = 0; i < srv->size; i++) {
            c = &srv->c[i];
            if(c->sockfd < 0) {
                if(c->time + RP_TIMEOUT < t) {
                    c->time = t;
                    /* try connect to server */
                    if(rp_server_connect(c) != NULL) {
                        e.data = c;
                        e.events = RP_EVENT_READ | RP_EVENT_WRITE;
                        eh.add(&eh, c->sockfd, &e);
                    }
                }
            } else {
                server = c->data;
                if(c->settings.ping > 0 && c->time + c->settings.ping < t && server->client == NULL) {
                    c->time = t;
                    c->flags |= RP_MAINTENANCE;
                    server->buffer.r = server->buffer.w = 0;
                    server->buffer.used = sprintf(server->buffer.s.data, "*1\r\n$4\r\nPING\r\n");
                    e.data = c;
                    e.events = RP_EVENT_WRITE;
                    eh.add(&eh, c->sockfd, &e);
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

void rp_connection_close(rp_connection_t *c, rp_event_handler_t *eh, rp_connection_pool_t *srv)
{
    int i;
    rp_event_t e;
    rp_client_t *client;
    rp_server_t *server;

    if(c->flags & RP_CLIENT) {
        client = c->data;
        if(client->server != NULL) {
            return;
        }
        e.data = c;
        e.events = RP_EVENT_READ | RP_EVENT_WRITE;
        eh->del(eh, c->sockfd, &e);
        eh->events[c->sockfd].events = 0;
        eh->events[c->sockfd].data = NULL;
        close(c->sockfd);
        /* free resources */
        free(client->buffer.s.data);
        free(c->data);
        free(c);
    } else {
        if(master == c) {
            master = NULL;
            for(i = 0; i < srv->size; i++) {
                c->time = 0;
                if(srv->c[i].flags & RP_SERVER) {
                    server = srv->c[i].data;
                    server->master = NULL;
                }
            }
        }
        server = c->data;
        /* close clients connections */
        while((server->client = rp_queue_shift(&server->queue)) != NULL) {
            client = server->client->data;
            client->server = NULL;
            rp_connection_close(server->client, eh, srv);
        }
        if(c->flags & RP_ESTABLISHED) {
            syslog(LOG_ERR, "Close connection to %s:%d",
                c->settings.address.data, c->settings.port);
        }
        e.data = c;
        e.events = RP_EVENT_READ | RP_EVENT_WRITE;
        eh->del(eh, c->sockfd, &e);
        eh->events[c->sockfd].events = 0;
        eh->events[c->sockfd].data = NULL;
        close(c->sockfd);
        c->sockfd = -1;
        time(&c->time);
        server->master = NULL;
        server->buffer.r = server->buffer.w = server->buffer.used = 0;
        server->in.used = server->out.used = 0;
        server->in.r = server->out.w = 0;
        c->flags = RP_SERVER;
    }
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
                server->buffer.s.length = 0;
                return NULL;
            }
            server->buffer.r = server->buffer.w = server->buffer.used = 0;
        }
        server->client = NULL;
        server->master = NULL;
        server->queue.size = 0;
        server->queue.first = server->queue.last = NULL;
        server->in.s.data = server->out.s.data = NULL;
        server->in.s.length = server->out.s.length = RP_NULL_STRLEN;
        server->in.used = server->out.used = 0;
        server->in.r = server->out.w = 0;
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

rp_connection_t *rp_connection_accept(rp_connection_t *l)
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
        client->buffer.s.length = 0;
    }
    client->cmd.argc = RP_NULL_STRLEN - 1;
    client->server = NULL;
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
        c->flags |= RP_MASTER;
        m = c;
    } else {
        server->buffer.used = sprintf(server->buffer.s.data,
            "*3\r\n$7\r\nSLAVEOF\r\n$%d\r\n%s\r\n$%d\r\n%s\r\n",
            m->settings.address.length, m->settings.address.data,
            sprintf(b, "%u", m->settings.port), b);
        c->flags &= ~RP_MASTER;
    }
    c->flags |= RP_MAINTENANCE;
    server->master = m;
    server->buffer.r = server->buffer.w = 0;
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

int rp_request_parse(rp_buffer_t *buf, rp_command_t *cmd)
{
    int i;
    char *ptr;
    rp_string_t s;

    s.data = &buf->s.data[buf->r];
    s.length = buf->used - buf->r;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        buf->r = ptr - buf->s.data + 2;
        if(cmd->argc < RP_NULL_STRLEN) {
            cmd->txt.data = s.data;
            if(*s.data != RP_MULTI_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) <= 0 || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->i = 0;
            cmd->argc = i;
            cmd->argv.data = NULL;
            cmd->argv.length = RP_NULL_STRLEN - 1;
        } else if(cmd->argv.length < RP_NULL_STRLEN) {
            if(*s.data != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->argv.length = i;
            cmd->argv.data = NULL;
        } else {
            if(cmd->argv.data == NULL) {
                cmd->argv.data = s.data;
            }
            i = ptr - cmd->argv.data;
            if(cmd->argv.length == RP_NULL_STRLEN || cmd->argv.length == i) {
                if(!cmd->i) {
                    cmd->name.data = cmd->argv.data;
                    cmd->name.length = cmd->argv.length;
                }
                if(++cmd->i == cmd->argc) {
                    cmd->txt.length = ptr - cmd->txt.data + 2;
                    return RP_SUCCESS;
                }
                cmd->argv.data = NULL;
                cmd->argv.length = RP_NULL_STRLEN - 1;
            } else if(i > cmd->argv.length) {
                return RP_FAILURE;
            }
        }
        s.data = &buf->s.data[buf->r];
        s.length = buf->used - buf->r;
    }
    return RP_UNKNOWN;
}

int rp_reply_parse(rp_buffer_t *buf, rp_command_t *cmd)
{
    int i;
    char *ptr;
    rp_string_t s;

    s.data = &buf->s.data[buf->r];
    s.length = buf->used - buf->r;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        buf->r = ptr - buf->s.data + 2;
        if(cmd->argc < RP_NULL_STRLEN) {
            cmd->i = 0;
            switch(*s.data) {
                case RP_MULTI_BULK_PREFIX:
                    s.data++;
                    s.length--;
                    if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                        return RP_FAILURE;
                    } else if(i <= 0) {
                        return RP_SUCCESS;
                    }
                    cmd->argc = i;
                    cmd->argv.data = NULL;
                    cmd->argv.length = RP_NULL_STRLEN - 1;
                    break;
                case RP_BULK_PREFIX:
                    s.data++;
                    s.length--;
                    if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                        return RP_FAILURE;
                    } else if(i < 0) {
                        return RP_SUCCESS;
                    }
                    cmd->argc = 1;
                    cmd->argv.data = NULL;
                    cmd->argv.length = i;
                    break;
                case RP_INTEGER_PREFIX:
                    s.data++;
                    s.length--;
                    i = rp_strtol(&s);
                    if(s.data != ptr) {
                        return RP_FAILURE;
                    }
                case RP_STATUS_PREFIX:
                case RP_ERROR_PREFIX:
                    return RP_SUCCESS;
                default:
                    return RP_FAILURE;
            }
        } else if(cmd->argv.length < RP_NULL_STRLEN) {
            if(*s.data != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->argv.length = i;
            cmd->argv.data = NULL;
        } else {
            if(cmd->argv.data == NULL) {
                cmd->argv.data = s.data;
            }
            i = ptr - cmd->argv.data;
            if(cmd->argv.length == RP_NULL_STRLEN || cmd->argv.length == i) {
                if(++cmd->i == cmd->argc) {
                    return RP_SUCCESS;
                }
                cmd->argv.data = NULL;
                cmd->argv.length = RP_NULL_STRLEN - 1;
            } else if(i > cmd->argv.length) {
                return RP_FAILURE;
            }
        }
        s.data = &buf->s.data[buf->r];
        s.length = buf->used - buf->r;
    }
    return RP_UNKNOWN;
}
