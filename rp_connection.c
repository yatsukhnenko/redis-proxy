#include "rp_socket.h"
#include "rp_connection.h"

static rp_connection_t *master = NULL;

int rp_connection_handler_loop(rp_connection_t *l, rp_event_handler_t *eh, rp_connection_pool_t *s)
{
    int i, j;
    time_t t;
    rp_event_t e;
    rp_string_t *err;
    rp_client_t *client;
    rp_server_t *server;
    rp_queue_t qget, qset;
    struct timeval timeout;
    rp_connection_t *c, *a;

    /* initialize queue */
    if(rp_queue_init(&qget, RP_CONCURRENT_CONNECTIONS) == NULL ||
        rp_queue_init(&qset, RP_CONCURRENT_CONNECTIONS) == NULL
    ) {
        return EXIT_FAILURE;
    }

    /* handle events on listen socket */
    e.data = l;
    e.events = RP_EVENT_READ;
    if(eh->add(eh, l->sockfd, &e) != RP_SUCCESS) {
        return EXIT_FAILURE;
    }

    /* main loop */
    for(;;) {
        time(&t);
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        for(i = 0; i < eh->wait(eh, &timeout); i++) {
            a = eh->ready[i].data;
            a->time = t;
            if(a->flags & RP_CLIENT) {
                client = a->data;
                if(eh->ready[i].events & RP_EVENT_READ) {
                    if(rp_recv(a->sockfd, &client->buffer) != RP_SUCCESS) {
                        rp_connection_close(a, eh, s);
                        continue;
                    }
                    if((j = rp_request_parse(&client->buffer, &client->cmd)) != RP_UNKNOWN) {
                        e.data = a;
                        e.events = RP_EVENT_READ;
                        eh->del(eh, a->sockfd, &e);
                        err = NULL;
                        if(j != RP_SUCCESS) {
                            if((err = rp_string("-ERR syntax error")) == NULL) {
                                rp_connection_close(a, eh, s);
                                continue;
                            }
                        } else if((client->cmd.proto = rp_command_lookup(client->cmd.argv)) == NULL) {
                            if((err = rp_string("-ERR unknown command '%s'", client->cmd.argv)) == NULL) {
                                rp_connection_close(a, eh, s);
                                continue;
                            }
                        } else if(!(a->flags & RP_AUTHENTICATED) && !(client->cmd.proto->flags & RP_WITHOUT_AUTH)) {
                            if((err = rp_string("-ERR operation not permitted")) == NULL) {
                                rp_connection_close(a, eh, s);
                                continue;
                            }
                        } else if((client->cmd.proto->argc < 0 && client->cmd.argc < abs(client->cmd.proto->argc))
                            || (client->cmd.proto->argc > 0 && client->cmd.argc != client->cmd.proto->argc)) {
                            if((err = rp_string("-ERR wrong number of arguments for '%s' command", client->cmd.argv)) == NULL) {
                                rp_connection_close(a, eh, s);
                                continue;
                            }
                        }
                        if(err != NULL) {
                            client->buffer.r = client->buffer.w = 0;
                            client->buffer.used = sprintf(client->buffer.s.data, "%.*s\r\n", err->length, err->data);
                            e.events = RP_EVENT_WRITE;
                            eh->add(eh, a->sockfd, &e);
                            a->flags |= RP_ALREADY;
                            free(err->data);
                            free(err);
                            continue;
                        }
                        if(client->cmd.proto->flags & RP_LOCAL_COMMAND) {
                            client->cmd.proto->handler(a);
                            e.events = RP_EVENT_WRITE;
                            eh->add(eh, a->sockfd, &e);
                            continue;
                        } else if(client->cmd.proto->flags & RP_MASTER_COMMAND) {
                            if((c = master) == NULL) {
                                for(j = 0; j < s->size; j++) {
                                    s->c[j].time = 0;
                                }
                            }
                        } else {
                            c = rp_server_lookup(s);
                        }
                        if(c == NULL) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        client->server = c;
                        server = c->data;
                        if(!(c->flags & RP_MAINTENANCE) && server->client == NULL) {
                            server->client = a;
                            e.data = c;
                            e.events = RP_EVENT_WRITE;
                            eh->add(eh, c->sockfd, &e);
                        } else {
                            rp_queue_push(client->cmd.proto->flags & RP_MASTER_COMMAND ? &qset : &qget, a);
                        }
                    }
                }
                if(eh->ready[i].events & RP_EVENT_WRITE) {
                    if(rp_send(a->sockfd, &client->buffer) != RP_SUCCESS) {
                        rp_connection_close(a, eh, s);
                        continue;
                    }
                    if(client->buffer.w == client->buffer.used && a->flags & RP_ALREADY) {
                        if(a->flags & RP_SHUTDOWN) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        e.data = a;
                        e.events = RP_EVENT_WRITE;
                        eh->del(eh, a->sockfd, &e);
                        client->buffer.r = client->buffer.w = client->buffer.used = 0;
                        client->cmd.argc = RP_NULL_STRLEN - 1;
                        free(client->cmd.argv);
                        client->cmd.argv = NULL;
                        e.data = a;
                        e.events = RP_EVENT_READ;
                        eh->add(eh, a->sockfd, &e);
                    }
                }
            } else if(a->flags & RP_SERVER) {
                server = a->data;
                if(eh->ready[i].events & RP_EVENT_READ) {
                    if(server->client != NULL) {
                        c = server->client;
                        client = c->data;
                        if(rp_recv(a->sockfd, &client->buffer) != RP_SUCCESS) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        if(!(c->flags & RP_INPROGRESS)) {
                            c->flags |= RP_INPROGRESS;
                            e.data = c;
                            e.events = RP_EVENT_WRITE;
                            eh->add(eh, c->sockfd, &e);
                        }
                        if((j = rp_reply_parse(&client->buffer, &client->cmd)) != RP_UNKNOWN) {
                            if(j == RP_SUCCESS) {
                                c->flags |= RP_ALREADY;
                                client->server = NULL;
                                e.data = a;
                                e.events = RP_EVENT_READ;
                                eh->del(eh, a->sockfd, &e);
                                if(a->flags & RP_MASTER) {
                                    if((server->client = rp_queue_shift(&qset)) == NULL) {
                                        server->client = rp_queue_shift(&qget);
                                    }
                                } else {
                                    server->client = rp_queue_shift(&qget);
                                }
                                if(server->client != NULL) {
                                    e.data = a;
                                    e.events = RP_EVENT_WRITE;
                                    eh->add(eh, a->sockfd, &e);
                                }
                            } else {
                                rp_connection_close(a, eh, s);
                                continue;
                            }
                        }
                    } else {
                        if(rp_recv(a->sockfd, &server->buffer) != RP_SUCCESS) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        if(*server->buffer.s.data == RP_STATUS_PREFIX) {
                            if(strstr(server->buffer.s.data, "\r\n") != NULL) {
                                /* all data transmitted from server */
                                a->flags &= ~RP_MAINTENANCE;
                                e.data = a;
                                e.events = RP_EVENT_READ;
                                eh->del(eh, a->sockfd, &e);
                                if(a->flags & RP_MASTER) {
                                    if((server->client = rp_queue_shift(&qset)) == NULL) {
                                        server->client = rp_queue_shift(&qget);
                                    }
                                } else {
                                    server->client = rp_queue_shift(&qget);
                                }
                                if(server->client != NULL) {
                                    /* handle next client */
                                    e.data = a;
                                    e.events = RP_EVENT_WRITE;
                                    eh->add(eh, a->sockfd, &e);
                                }
                            }
                        } else {
                            syslog(LOG_NOTICE, "%s", server->buffer.s.data);
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                    }
                }
                if(eh->ready[i].events & RP_EVENT_WRITE) {
                    if(!(a->flags & RP_ESTABLISHED)) {
                        /* check previous connection attempt */
                        if(rp_server_connect(a) == NULL) {
                            rp_connection_close(a, eh, s);
                        }
                        continue;
                    }
                    if(server->client != NULL) {
                        c = server->client;
                        client = c->data;
                        if(rp_send(a->sockfd, &client->buffer) != RP_SUCCESS) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        if(client->buffer.w == client->buffer.used) {
                            e.data = a;
                            e.events = RP_EVENT_WRITE;
                            eh->del(eh, a->sockfd, &e);
                            c->flags &= ~RP_ALREADY;
                            c->flags &= ~RP_INPROGRESS;
                            client->buffer.r = client->buffer.w = client->buffer.used = 0;
                            client->cmd.argc = RP_NULL_STRLEN - 1;
                            e.data = a;
                            e.events = RP_EVENT_READ;
                            eh->add(eh, a->sockfd, &e);
                        }
                    } else {
                        if(master == NULL) {
                            master = a;
                            rp_set_slaveof(a, NULL);
                            syslog(LOG_INFO, "Set %s:%d slave of no one", a->hr.address.data, a->hr.port);
                        } else if(!(a->flags & RP_MASTER)) {
                            if(master->flags & RP_MAINTENANCE) {
                                /* master is not ready */
                                continue;
                            } else if(server->master != master) {
                                rp_set_slaveof(a, master);
                                syslog(LOG_INFO, "Set %s:%d slave of %s:%d",
                                    a->hr.address.data, a->hr.port,
                                    master->hr.address.data, master->hr.port);
                            }
                        }
                        /* send request to server */
                        if(rp_send(a->sockfd, &server->buffer) != RP_SUCCESS) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        if(server->buffer.w == server->buffer.used) {
                            /* all data transmitted to server */
                            e.data = a;
                            e.events = RP_EVENT_WRITE;
                            eh->del(eh, a->sockfd, &e);
                            server->buffer.r = server->buffer.w = server->buffer.used = 0;
                            e.data = a;
                            e.events = RP_EVENT_READ;
                            eh->add(eh, a->sockfd, &e);
                        }
                    }
                }
            } else {
                /* accept connection from client */
                if((c = rp_connection_accept(l)) != NULL) {
                    e.data = c;
                    e.events = RP_EVENT_READ;
                    eh->add(eh, c->sockfd, &e);
                }
            }
        }
        for(i = 0; i < s->size; i++) {
            c = &s->c[i];
            if(c->sockfd < 0) {
                if(c->time + RP_TIMEOUT < t) {
                    c->time = t;
                    /* try connect to server */
                    if(rp_server_connect(c) != NULL) {
                        e.data = c;
                        e.events = RP_EVENT_WRITE;
                        eh->add(eh, c->sockfd, &e);
                    }
                }
            } else {
                server = c->data;
                if(c->ping > 0 && c->time + c->ping < t && server->client == NULL) {
                    c->time = t;
                    c->flags |= RP_MAINTENANCE;
                    server->buffer.r = server->buffer.w = 0;
                    server->buffer.used = sprintf(server->buffer.s.data, "*1\r\n$4\r\nPING\r\n");
                    e.data = c;
                    e.events = RP_EVENT_WRITE;
                    eh->add(eh, c->sockfd, &e);
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

void rp_connection_close(rp_connection_t *c, rp_event_handler_t *eh, rp_connection_pool_t *s)
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
        close(c->sockfd);
        /* free resources */
        free(client->buffer.s.data);
        free(client->cmd.argv);
        free(c->data);
        free(c);
    } else {
        if(master == c) {
            master = NULL;
            for(i = 0; i < s->size; i++) {
                c->time = 0;
                if(s->c[i].flags & RP_SERVER) {
                    server = s->c[i].data;
                    server->master = NULL;
                }
            }
        }
        server = c->data;
        /* close clients connections */
        if(server->client != NULL) {
            client = server->client->data;
            client->server = NULL;
            rp_connection_close(server->client, eh, s);
        }
        server->client = NULL;
        syslog(LOG_ERR, "Close connection to %s:%d", c->hr.address.data, c->hr.port);
        e.data = c;
        e.events = RP_EVENT_READ | RP_EVENT_WRITE;
        eh->del(eh, c->sockfd, &e);
        close(c->sockfd);
        c->sockfd = -1;
        time(&c->time);
        server->master = NULL;
        server->buffer.r = server->buffer.w = server->buffer.used = 0;
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
        server->client = NULL;
        server->master = NULL;
        if(server->buffer.s.data == NULL) {
            /* initialize buffer */
            server->buffer.s.length = RP_BUFFER_SIZE;
            if((server->buffer.s.data = malloc(server->buffer.s.length)) == NULL) {
                server->buffer.s.length = 0;
                return NULL;
            }
            server->buffer.r = server->buffer.w = server->buffer.used = 0;
        }
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
                    c->hr.address.data, c->hr.port, strerror(errno));
                close(c->sockfd);
                c->sockfd = -1;
                return NULL;
            }
        }
    } else if(!(c->flags & RP_ESTABLISHED)) {
        /* check previous connection attempt state */
        if(getsockopt(c->sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen)) {
            syslog(LOG_ERR, "getsockopt at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        } else if(optval) {
            if(optval != EALREADY) {
                syslog(LOG_NOTICE, "Could not connect to Redis at %s:%d: %s",
                    c->hr.address.data, c->hr.port, strerror(optval));
                close(c->sockfd);
                c->sockfd = -1;
                return NULL;
            }
        } else {
            /* connection has been established successfully */
            c->flags |= RP_ESTABLISHED;
            syslog(LOG_NOTICE, "Successfully connected to %s:%d", c->hr.address.data, c->hr.port);
        }
    }
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
    client->cmd.argv = NULL;
    client->server = NULL;
    c->data = client;
    c->sockfd = sockfd;
    c->flags = RP_CLIENT;
    c->auth.data = l->auth.data;
    c->auth.length = l->auth.length;
    if(l->auth.data == NULL) {
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
            (int)m->hr.address.length, m->hr.address.data,
            sprintf(b, "%u", m->hr.port), b);
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
            if((cmd->argv = calloc(i, sizeof(rp_string_t))) == NULL) {
                syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
                return RP_FAILURE;
            }
            for(i = 0; i < cmd->argc; i++) {
                cmd->argv[i].data = NULL;
                cmd->argv[i].length = RP_NULL_STRLEN - 1;
            }
        } else if(cmd->argv[cmd->i].length < RP_NULL_STRLEN) {
            if(*s.data != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->argv[cmd->i].length = i;
            cmd->argv[cmd->i].data = NULL;
        } else {
            if(cmd->argv[cmd->i].data == NULL) {
                cmd->argv[cmd->i].data = s.data;
            }
            i = ptr - cmd->argv[cmd->i].data;
            if(cmd->argv[cmd->i].length == RP_NULL_STRLEN || cmd->argv[cmd->i].length == i) {
                if(++cmd->i == cmd->argc) {
                    return RP_SUCCESS;
                }
            } else if(i > cmd->argv[cmd->i].length) {
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
                    cmd->argv->length = RP_NULL_STRLEN - 1;
                    break;
                case RP_BULK_PREFIX:
                    s.data++;
                    s.length--;
                    if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                        return RP_FAILURE;
                    } else if(i <= 0) {
                        return RP_SUCCESS;
                    }
                    cmd->argc = 1;
                    cmd->argv->data = NULL;
                    cmd->argv->length = i;
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
        } else if(cmd->argv->length < RP_NULL_STRLEN) {
            if(*s.data != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            s.data++;
            s.length--;
            if((i = rp_strtol(&s)) < RP_NULL_STRLEN || s.data != ptr) {
                return RP_FAILURE;
            }
            cmd->argv->length = i;
            cmd->argv->data = NULL;
        } else {
            if(cmd->argv->data == NULL) {
                cmd->argv->data = s.data;
            }
            i = ptr - cmd->argv->data;
            if(cmd->argv->length == RP_NULL_STRLEN || i == cmd->argv->length) {
                if(++cmd->i == cmd->argc) {
                    return RP_SUCCESS;
                }
                cmd->argv->length = RP_NULL_STRLEN - 1;
            } else if(i > cmd->argv->length) {
                return RP_FAILURE;
            }
        }
        s.data = &buf->s.data[buf->r];
        s.length = buf->used - buf->r;
    }
    return RP_UNKNOWN;
}
