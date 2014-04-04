#ifndef _RP_EPOLL_H_
#define _RP_EPOLL_H_

#include <sys/epoll.h>
#include "rp_event.h"

typedef struct {
    int epfd;
    struct epoll_event *events;
} rp_epoll_data_t;

rp_event_handler_t *rp_epoll_init(rp_event_handler_t *eh);
int rp_epoll_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
int rp_epoll_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
void rp_epoll_wait(struct rp_event_handler *eh, struct timeval *timeout);

#endif /* _RP_EPOLL_H_ */
