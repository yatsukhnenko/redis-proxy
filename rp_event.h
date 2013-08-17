#ifndef _RP_EVENT_H_
#define _RP_EVENT_H_

#include <sys/time.h>
#include <sys/epoll.h>
#include "rp_common.h"

#define RP_EVENT_READ  1
#define RP_EVENT_WRITE 2

typedef struct {
    unsigned char events;
    void *data;
} rp_event_t;

typedef struct {
    int epfd;
    struct epoll_event *epoll_events;
    size_t maxevents;
    rp_event_t **ready;
    rp_event_t *events;
} rp_event_handler_t;

rp_event_handler_t *rp_event_handler_init(rp_event_handler_t *eh, size_t maxevents);
int rp_event_add(rp_event_handler_t *eh, int sockfd, rp_event_t *e);
int rp_event_del(rp_event_handler_t *eh, int sockfd);
int rp_event_wait(rp_event_handler_t *eh, struct timeval *timeout);

#endif /* _RP_EVENT_H_ */
