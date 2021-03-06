#ifndef _RP_KQUEUE_H_
#define _RP_KQUEUE_H_

#include <sys/types.h>
#include <sys/event.h>
#include "rp_event.h"

typedef struct {
    int kqfd;
    struct kevent *events;
} rp_kqueue_data_t;

rp_event_handler_t *rp_kqueue_init(rp_event_handler_t *eh);
int rp_kqueue_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
int rp_kqueue_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
void rp_kqueue_wait(struct rp_event_handler *eh, struct timeval *timeout);

#endif /* _RP_KQUEUE_H_ */
