#include "rp_event.h"

rp_event_handler_t *rp_event_handler_init(rp_event_handler_t *eh, size_t maxevents)
{
    int alloc = 0;

    if(eh == 0) {
        if((eh = malloc(sizeof(rp_event_handler_t))) == NULL) {
            syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
            return NULL;
        }
        alloc = 1;
    }
    if((eh->epfd = epoll_create(maxevents)) < 0) {
        syslog(LOG_ERR, "epoll_create at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        if(alloc) {
            free(eh);
        }
        return NULL;
    }
    if((eh->epoll_events = calloc(maxevents, sizeof(struct epoll_event))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        if(alloc) {
            free(eh);
        }
        return NULL;
    }
    if((eh->events = calloc(maxevents, sizeof(rp_event_t))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(eh->epoll_events);
        if(alloc) {
            free(eh);
        }
        return NULL;
    }
    memset(eh->events, 0, maxevents * sizeof(rp_event_t));
    if((eh->ready = calloc(maxevents, sizeof(rp_event_t *))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(eh->epoll_events);
        free(eh->events);
        if(alloc) {
            free(eh);
        }
        return NULL;
    }
    eh->maxevents = maxevents;
    return eh;
}

int rp_event_add(rp_event_handler_t *eh, int sockfd, rp_event_t *e)
{
    int op;
    struct epoll_event event;

    op = !eh->events[sockfd].events ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;
    e->events |= eh->events[sockfd].events;

    event.data.fd = sockfd;
    event.events = EPOLLERR | EPOLLHUP;
    if(e->events & RP_EVENT_READ) {
        event.events |= EPOLLIN;
    }
    if(e->events & RP_EVENT_WRITE) {
        event.events |= EPOLLOUT;
    }
    if(epoll_ctl(eh->epfd, op, sockfd, &event)) {
        syslog(LOG_ERR, "epoll_ctl at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    eh->events[sockfd].events = e->events;
    eh->events[sockfd].data = e->data;
    return RP_SUCCESS;
}

int rp_event_del(rp_event_handler_t *eh, int sockfd)
{
    struct epoll_event event;

    if(eh->events[sockfd].events) {
        event.events = 0;
        event.data.fd = sockfd;
        if(epoll_ctl(eh->epfd, EPOLL_CTL_DEL, sockfd, &event)) {
            syslog(LOG_ERR, "epoll_ctl at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
            return RP_FAILURE;
        }
        eh->events[sockfd].events = 0;
        eh->events[sockfd].data = NULL;
    }
    return RP_SUCCESS;
}

int rp_event_wait(rp_event_handler_t *eh, struct timeval *timeout)
{
    int i, ready;
    struct epoll_event *epev;

    i = timeout != NULL ? timeout->tv_sec * 1000 + timeout->tv_usec / 1000 : -1;
    if((ready = epoll_wait(eh->epfd, eh->epoll_events, eh->maxevents, i)) < 0) {
        syslog(LOG_ERR, "epoll_wait at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
    } else if(ready > 0) {
        for(i = 0; i < ready; i++) {
            epev = &eh->epoll_events[i];
            eh->ready[i] = &eh->events[epev->data.fd];

            if(epev->events & (EPOLLERR | EPOLLHUP)) {
                eh->ready[i]->events = RP_EVENT_READ;
                continue;
            } else {
                if(epev->events & EPOLLIN) {
                    eh->ready[i]->events |= RP_EVENT_READ;
                }
                if(epev->events & EPOLLOUT) {
                    eh->ready[i]->events |= RP_EVENT_WRITE;
                }
            }
        }
    }
    return ready;
}
