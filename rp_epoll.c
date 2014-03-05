#include "rp_epoll.h"
#ifdef RP_HAVE_EPOLL

rp_event_handler_t *rp_epoll_init(rp_event_handler_t *eh, size_t maxevents)
{
    rp_epoll_data_t *ed = NULL;

    if((ed = malloc(sizeof(rp_epoll_data_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    if((ed->events = calloc(maxevents, sizeof(struct epoll_event))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(ed);
        return NULL;
    }
    if((ed->epfd = epoll_create(maxevents)) < 0) {
        syslog(LOG_ERR, "epoll_create at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(ed->events);
        free(ed);
        return NULL;
    }

    eh->data = ed;
    eh->add = rp_epoll_add;
    eh->del = rp_epoll_del;
    eh->wait = rp_epoll_wait;
    syslog(LOG_INFO, "using 'epoll' I/O multiplexing mechanism");

    return eh;
}


int rp_epoll_add(rp_event_handler_t *eh, int sockfd, rp_event_t *e)
{
    int op;
    struct epoll_event event;
    rp_epoll_data_t *ed = eh->data;

    event.events = 0;
    if(e->events & RP_EVENT_READ) {
        event.events |= EPOLLIN;
        if(eh->events[sockfd].events & RP_EVENT_READ) {
            e->events &= ~RP_EVENT_READ;
        }
    }
    if(e->events & RP_EVENT_WRITE) {
        event.events |= EPOLLOUT;
        if(eh->events[sockfd].events & RP_EVENT_WRITE) {
            e->events &= ~RP_EVENT_WRITE;
        }
    }
    if(e->events) {
        event.data.fd = sockfd;
        event.events |= EPOLLERR | EPOLLHUP;
        op = !eh->events[sockfd].events ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
        if(epoll_ctl(ed->epfd, op, sockfd, &event)) {
            syslog(LOG_ERR, "epoll_ctl at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
            return RP_FAILURE;
        }
        eh->events[sockfd].events |= e->events;
        eh->events[sockfd].data = e->data;
    }
    return RP_SUCCESS;
}

int rp_epoll_del(rp_event_handler_t *eh, int sockfd, rp_event_t *e)
{
    int op;
    struct epoll_event event;
    rp_epoll_data_t *ed = eh->data;

    e->events &= eh->events[sockfd].events;
    if(e->events) {
        event.events = 0;
        if(eh->events[sockfd].events & RP_EVENT_READ) {
            if(e->events & RP_EVENT_READ) {
                e->events &= ~RP_EVENT_READ;
            } else {
                e->events |= RP_EVENT_READ;
                event.events |= EPOLLIN;
            }
        } else {
            e->events &= ~RP_EVENT_READ;
        }
        if(eh->events[sockfd].events & RP_EVENT_WRITE) {
            if(e->events & RP_EVENT_WRITE) {
                e->events &= ~RP_EVENT_WRITE;
            } else {
                e->events |= RP_EVENT_WRITE;
                event.events |= EPOLLOUT;
            }
        } else {
            e->events &= ~RP_EVENT_WRITE;
        }
        if(e->events != eh->events[sockfd].events) {
            event.data.fd = sockfd;
            event.events |= EPOLLERR | EPOLLHUP;
            op = !e->events ? EPOLL_CTL_DEL : EPOLL_CTL_MOD;
            if(epoll_ctl(ed->epfd, op, sockfd, &event)) {
                syslog(LOG_ERR, "epoll_ctl at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
                return RP_FAILURE;
            }
            eh->events[sockfd].events = e->events;
            if(!eh->events[sockfd].events) {
                eh->events[sockfd].data = NULL;
            }
        }
    }
    return RP_SUCCESS;
}

int rp_epoll_wait(rp_event_handler_t *eh, struct timeval *timeout)
{
    int i, ready;
    struct epoll_event *event;
    rp_epoll_data_t *ed = eh->data;

    i = timeout != NULL ? timeout->tv_sec * 1000 + timeout->tv_usec / 1000 : -1;
    if((ready = epoll_wait(ed->epfd, ed->events, eh->maxevents, i)) < 0) {
        syslog(LOG_ERR, "epoll_wait at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
    } else if(ready > 0) {
        for(i = 0; i < ready; i++) {
            event = &ed->events[i];
            eh->ready[i].events = 0;
            eh->ready[i].data = eh->events[event->data.fd].data;
            if(event->events & (EPOLLERR | EPOLLHUP)) {
                eh->ready[i].events = RP_EVENT_READ;
                continue;
            } else {
                if(event->events & EPOLLIN) {
                    eh->ready[i].events |= RP_EVENT_READ;
                }
                if(event->events & EPOLLOUT) {
                    eh->ready[i].events |= RP_EVENT_WRITE;
                }
            }
        }
    }

    return ready;
}

#endif /* RP_HAVE_EPOLL */
