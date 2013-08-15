#include "rp_event.h"

rp_event_handler_t *rp_event_handler_init(rp_event_handler_t *eh, size_t maxevents)
{
    int alloc = 0;
    if(eh == 0) {
        if((eh = malloc(sizeof(rp_event_handler_t))) == NULL) {
            syslog(LOG_ERR, "malloc at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
            return NULL;
        }
        alloc = 1;
    }
    if((eh->epfd = epoll_create(maxevents)) < 0) {
        syslog(LOG_ERR, "epoll_create at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        if(alloc) {
            free(eh);
        }
        return NULL;
    }
    if((eh->events = calloc(maxevents, sizeof(struct epoll_event))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        if(alloc) {
            free(eh);
        }
        return NULL;
    }
    if((eh->ready = calloc(maxevents, sizeof(rp_event_t))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
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
    struct epoll_event event;

    event.data.ptr = e->data;
    event.events = EPOLLERR | EPOLLHUP;
    if(e->events & RP_EVENT_READ) {
        event.events |= EPOLLIN;
    }
    if(e->events & RP_EVENT_WRITE) {
        event.events |= EPOLLOUT;
    }
    if(epoll_ctl(eh->epfd, EPOLL_CTL_ADD, sockfd, &event)) {
        syslog(LOG_ERR, "epoll_ctl at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    return RP_SUCCESS;
}

int rp_event_del(rp_event_handler_t *eh, int sockfd)
{
    struct epoll_event event;

    event.events = 0; event.data.ptr = NULL;
    if(epoll_ctl(eh->epfd, EPOLL_CTL_DEL, sockfd, &event)) {
        syslog(LOG_ERR, "epoll_ctl at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    return RP_SUCCESS;
}

int rp_event_wait(rp_event_handler_t *eh, struct timeval *timeout)
{
    int i, ready;

    i = timeout != NULL ? timeout->tv_sec * 1000 + timeout->tv_usec / 1000 : -1;
    if((ready = epoll_wait(eh->epfd, eh->events, eh->maxevents, i)) < 0) {
        syslog(LOG_ERR, "epoll_wait at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
    } else if(ready > 0) {
        for(i = 0; i < ready; i++) {
            eh->ready[i].data = eh->events[i].data.ptr;
            eh->ready[i].events = 0;
            if(eh->events[i].events & (EPOLLERR | EPOLLHUP)) {
                eh->ready[i].events = RP_EVENT_WRITE;
                continue;
            } else {
                if(eh->events[i].events & EPOLLIN) {
                    eh->ready[i].events |= RP_EVENT_READ;
                }
                if(eh->events[i].events & EPOLLOUT) {
                    eh->ready[i].events |= RP_EVENT_WRITE;
                }
            }
        }
    }
    return ready;
}
