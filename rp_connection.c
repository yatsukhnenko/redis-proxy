#include "rp_socket.h"
#include "rp_connection.h"

static rp_connection_t *master = NULL;

int rp_connection_handler_loop(rp_connection_t *l, rp_connection_pool_t *s)
{
    time_t t;
    rp_event_t e;
    int i, j, ready;
    rp_client_t *client;
    rp_server_t *server;
    struct timeval timeout;
    rp_connection_t *c, *a;
    rp_event_handler_t *eh = NULL;

    /* initialize event handler object */
    if((eh = rp_event_handler_init(NULL, RP_CONCURRENT_CONNECTIONS + s->size + 1)) == NULL) {
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

        for(i = 0; i < s->size; i++) {
            c = &s->c[i];
            server = c->data;
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
                if(c->time + RP_TIMEOUT < t && server->client == NULL) {
                    c->time = t;
                    c->flags |= RP_MAINTENANCE;
                    server->buffer.r = server->buffer.w = 0;
                    server->buffer.used = sprintf(server->buffer.s.data, "PING\r\n");
                    e.data = c;
                    e.events = RP_EVENT_WRITE;
                    eh->add(eh, c->sockfd, &e);
                }
            }
        }

        timeout.tv_sec = RP_TIMEOUT;
        timeout.tv_usec = 0;
        if((ready = eh->wait(eh, &timeout)) > 0) {
            for(i = 0; i < ready; i++) {
                a = eh->ready[i].data;
                a->time = t;
                if(a->flags & RP_CLIENT) {
                    client = a->data;
                    if(eh->ready[i].events & RP_EVENT_READ) {
                        if(rp_recv(a->sockfd, &client->buffer) != RP_SUCCESS) {
                            rp_connection_close(a, eh, s);
                            continue;
                        }
                        if((j = rp_request_parse(&client->buffer, &client->command)) != RP_UNKNOWN) {
                            if(j == RP_SUCCESS) {
                                if(client->command.proto->flags & RP_LOCAL_COMMAND) {
                                    e.data = a;
                                    e.events = RP_EVENT_READ;
                                    eh->del(eh, a->sockfd, &e);
                                    client->command.proto->handler(a);
                                    e.data = a;
                                    e.events = RP_EVENT_WRITE;
                                    eh->add(eh, a->sockfd, &e);
                                    continue;
                                }
                                if(client->command.proto->flags & RP_MASTER_COMMAND) {
                                    c = master;
                                    if(master == NULL) {
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
                                e.data = a;
                                e.events = RP_EVENT_READ;
                                eh->del(eh, a->sockfd, &e);
                                client->server = c;
                                server = c->data;
                                if(!(c->flags & RP_MAINTENANCE) && server->client == NULL) {
                                    server->client = a;
                                    e.data = c;
                                    e.events = RP_EVENT_WRITE;
                                    eh->add(eh, c->sockfd, &e);
                                } else {
                                    rp_queue_push(&server->queue, a);
                                }
                            } else {
                                e.data = a;
                                e.events = RP_EVENT_READ;
                                eh->del(eh, a->sockfd, &e);
                                client->buffer.r = client->buffer.w = 0;
                                client->buffer.used = sprintf(client->buffer.s.data, "-Protocol error\r\n");
                                e.data = a;
                                e.events = RP_EVENT_WRITE;
                                eh->add(eh, a->sockfd, &e);
                                a->flags |= RP_ALREADY;
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
                            client->command.argc = client->command.argv.length = RP_NULL_LEN - 1;
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
                            if((j = rp_reply_parse(&client->buffer, &client->command)) != RP_UNKNOWN) {
                                if(j == RP_SUCCESS) {
                                    c->flags |= RP_ALREADY;
                                    client->server = NULL;
                                    e.data = a;
                                    e.events = RP_EVENT_READ;
                                    eh->del(eh, a->sockfd, &e);
                                    if((server->client = rp_queue_shift(&server->queue)) != NULL) {
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
                                    if((server->client = rp_queue_shift(&server->queue)) != NULL) {
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
                                client->command.argc = client->command.argv.length = RP_NULL_LEN - 1;
                                e.data = a;
                                e.events = RP_EVENT_READ;
                                eh->add(eh, a->sockfd, &e);
                            }
                        } else {
                            if(master == NULL) {
                                master = a;
                                rp_set_slaveof(a, NULL);
                                syslog(LOG_INFO, "Set %s:%d slave of no one", a->hr.address, a->hr.port);
                            } else if(!(a->flags & RP_MASTER)) {
                                if(master->flags & RP_MAINTENANCE) {
                                    /* master is not ready */
                                    continue;
                                } else if(server->master != master) {
                                    rp_set_slaveof(a, master);
                                    syslog(LOG_INFO, "Set %s:%d slave of %s:%d",
                                        a->hr.address, a->hr.port,
                                        master->hr.address, master->hr.port);
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
                    if((c = rp_connection_accept(NULL, l->sockfd)) != NULL) {
                        e.data = c;
                        e.events = RP_EVENT_READ;
                        eh->add(eh, c->sockfd, &e);
                    }
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
        while((server->client = rp_queue_shift(&server->queue)) != NULL) {
            client = server->client->data;
            client->server = NULL;
            rp_connection_close(server->client, eh, s);
        }
        syslog(LOG_ERR, "Close connection to %s:%d", c->hr.address, c->hr.port);
        e.data = c;
        e.events = RP_EVENT_READ | RP_EVENT_WRITE;
        eh->del(eh, c->sockfd, &e);
        close(c->sockfd);
        c->sockfd = -1;
        time(&c->time);
        server->master = NULL;
        server->queue.current = server->queue.last = 0;
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
        if(!server->queue.size) {
            /* initialize queue */
            if(rp_queue_init(&server->queue, RP_CONCURRENT_CONNECTIONS) == NULL) {
                return NULL;
            }
        }
        if(server->buffer.s.data == NULL) {
            /* initialize buffer */
            server->buffer.s.length = RP_DOUBLE_BUFFER_SIZE;
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
                    c->hr.address, c->hr.port, strerror(errno));
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
                    c->hr.address, c->hr.port, strerror(optval));
                close(c->sockfd);
                c->sockfd = -1;
                return NULL;
            }
        } else {
            /* connection has been established successfully */
            c->flags |= RP_ESTABLISHED;
            syslog(LOG_NOTICE, "Successfully connected to %s:%d", c->hr.address, c->hr.port);
        }
    }
    return c;
}

rp_connection_t *rp_connection_accept(rp_connection_t *c, int sockfd)
{
    int s, alloc = 0;
    rp_client_t *client;
    struct sockaddr_in addr;
    socklen_t l = sizeof(addr);

    if((s = accept(sockfd, (struct sockaddr *)&addr, &l)) < 0) {
        syslog(LOG_ERR, "accept at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    if(c == NULL) {
        if((c = malloc(sizeof(rp_connection_t))) == NULL) {
            syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
            close(s);
            return NULL;
        }
        alloc = 1;
    }
    if((client = malloc(sizeof(rp_client_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(s);
        if(alloc) {
            free(c);
        }
        return NULL;
    }
    memset(client, 0, sizeof(rp_client_t));
    client->buffer.s.length = RP_DOUBLE_BUFFER_SIZE;
    if((client->buffer.s.data = malloc(client->buffer.s.length)) == NULL) {
        client->buffer.s.length = 0;
    }
    client->command.argc = client->command.argv.length = RP_NULL_LEN - 1;
    client->server = NULL;
    c->flags = RP_CLIENT;
    c->data = client;
    c->sockfd = s;
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
            (int)strlen(m->hr.address), m->hr.address,
            sprintf(b, "%u", m->hr.port), b);
        c->flags &= ~RP_MASTER;
    }
    c->flags |= RP_MAINTENANCE;
    server->master = m;
    server->buffer.r = server->buffer.w = 0;
}

rp_connection_t *rp_server_lookup(rp_connection_pool_t *s)
{
    int i = 0;

    while(i++ < s->size) {
        if(s->i == s->size) {
            s->i = 0;
        }
        if(s->c[i].sockfd >= 0 && s->c[s->i].flags & RP_ESTABLISHED) {
            break;
        }
        s->i++;
    }
    return i < s->size + 1 ? &s->c[s->i++] : NULL;
}

int rp_request_parse(rp_buffer_t *buffer, rp_command_t *command)
{
    int i;
    char *b, *ptr;
    rp_string_t s;

    s.data = &buffer->s.data[buffer->r];
    s.length = buffer->used - buffer->r;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        b = s.data;
        if(command->argc < RP_NULL_LEN) {
            command->i = 0;
            if(*b != RP_MULTI_BULK_PREFIX) {
                if((command->proto = rp_lookup_command(b, -1)) == NULL) {
                    return RP_FAILURE;
                }
                return RP_SUCCESS;
            } else {
                i = 0; b++;
                while(*b >= '0' && *b <= '9') {
                    i = i * 10 + *b++ - '0';
                }
                if(b != ptr)  {
                    return RP_FAILURE;
                }
                command->argc = i;
                command->argv.length = RP_NULL_LEN - 1;
            }
        } else if(command->argv.length < RP_NULL_LEN) {
            if(*b++ != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            if(*b == '-') {
                if(*++b != '1' || ++b != ptr) {
                    return RP_FAILURE;
                }
                command->argv.length = RP_NULL_LEN;
            } else {
                i = 0;
                while(*b >= '0' && *b <= '9') {
                    i = i * 10 + *b++ - '0';
                }
                if(b != ptr) {
                    return RP_FAILURE;
                }
                command->argv.length = i;
            }
            command->argv.ptr = NULL;
        } else {
            if(command->argv.ptr == NULL) {
                command->argv.ptr = b;
            }
            if(command->argv.length == RP_NULL_LEN || (i = ptr - command->argv.ptr) == command->argv.length) {
                if(!command->i++) {
                    if((command->proto = rp_lookup_command(command->argv.ptr, command->argv.length)) == NULL) {
                        return RP_FAILURE;
                    }
                }
                if(command->i >= command->argc) {
                    /* TODO: check argc */
                    return RP_SUCCESS;
                }
                command->argv.length = RP_NULL_LEN - 1;
            } else if(i > command->argv.length) {
                return RP_FAILURE;
            }
        }
        buffer->r = ptr - buffer->s.data + 2;
        s.data = &buffer->s.data[buffer->r];
        s.length = buffer->used - buffer->r;
    }
    return RP_UNKNOWN;
}

int rp_reply_parse(rp_buffer_t *buffer, rp_command_t *command)
{
    int i;
    char *b, *ptr;
    rp_string_t s;

    s.data = &buffer->s.data[buffer->r];
    s.length = buffer->used - buffer->r;
    while((ptr = rp_strstr(&s, "\r\n")) != NULL) {
        b = s.data;
        if(command->argc < RP_NULL_LEN) {
            command->i = 0;
            switch(*b++) {
                case RP_MULTI_BULK_PREFIX:
                    if(*b == '-') {
                        if(*++b != '1' || ++b != ptr) {
                            return RP_FAILURE;
                        }
                        i = RP_NULL_LEN;
                    } else {
                        i = 0;
                        while(*b >= '0' && *b <= '9') {
                            i = i * 10 + *b++ - '0';
                        }
                        if(b != ptr) {
                            return RP_FAILURE;
                        }
                    }
                    if(!i || i == RP_NULL_LEN) {
                        return RP_SUCCESS;
                    }
                    command->argc = i;
                    command->argv.length = RP_NULL_LEN - 1;
                    break;
                case RP_BULK_PREFIX:
                    if(*b == '-') {
                        if(*++b != '1' || ++b != ptr) {
                            return RP_FAILURE;
                        }
                        return RP_SUCCESS;
                    }
                    i = 0;
                    while(*b >= '0' && *b <= '9') {
                        i = i * 10 + *b++ - '0';
                    }
                    if(b != ptr) {
                        return RP_FAILURE;
                    }
                    command->argc = 1;
                    command->argv.ptr = NULL;
                    command->argv.length = i;
                    break;
                case RP_STATUS_PREFIX:
                case RP_ERROR_PREFIX:
                case RP_INTEGER_PREFIX:
                    if(ptr - b + 2 != buffer->used - 1) {
                        return RP_FAILURE;
                    }
                    return RP_SUCCESS;
                default:
                    return RP_FAILURE;
            }
        } else if(command->argv.length < RP_NULL_LEN) {
            if(*b++ != RP_BULK_PREFIX) {
                return RP_FAILURE;
            }
            if(*b == '-') {
                if(*++b != '1' || ++b != ptr) {
                    return RP_FAILURE;
                }
                command->argv.length = RP_NULL_LEN;
            } else {
                i = 0;
                while(*b >= '0' && *b <= '9') {
                    i = i * 10 + *b++ - '0';
                }
                if(b != ptr) {
                    return RP_FAILURE;
                }
                command->argv.length = i;
            }
            command->argv.ptr = NULL;
        } else {
            if(command->argv.ptr == NULL) {
                command->argv.ptr = b;
            }
            if(command->argv.length == RP_NULL_LEN || (i = ptr - command->argv.ptr) == command->argv.length) {
                if(++command->i == command->argc) {
                    return RP_SUCCESS;
                }
                command->argv.length = RP_NULL_LEN - 1;
            } else if(i > command->argv.length) {
                return RP_FAILURE;
            }
        }
        buffer->r = ptr - buffer->s.data + 2;
        s.data = &buffer->s.data[buffer->r];
        s.length = buffer->used - buffer->r;
    }
    return RP_UNKNOWN;
}
