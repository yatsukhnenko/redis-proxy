#ifndef _RP_KQUEUE_H_
#define _RP_KQUEUE_H_

#include "rp_event.h"
#ifdef RP_HAVE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>

typedef struct {
    int kqfd;
    struct kevent *events;
} rp_kqueue_data_t;

rp_event_handler_t *rp_kqueue_init(rp_event_handler_t *eh);
int rp_kqueue_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
int rp_kqueue_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
int rp_kqueue_wait(struct rp_event_handler *eh, struct timeval *timeout);

#endif /* RP_HAVE_KQUEUE */
#endif /* _RP_KQUEUE_H_ */
