#ifndef _RP_EVENT_H_
#define _RP_EVENT_H_

#include <sys/time.h>

#include "rp_common.h"

#define RP_EVENT_READ  1
#define RP_EVENT_WRITE 2

typedef struct {
    short events;
    void *data;
} rp_event_t;

typedef struct rp_event_handler {
    void *data;
    size_t maxevents;
    rp_event_t *events;
    int (*add)(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
    int (*del)(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
    void (*poll)(struct rp_event_handler *eh, struct timeval *timeout);
} rp_event_handler_t;

rp_event_handler_t *rp_event_handler_init(rp_event_handler_t *eh);

#endif /* _RP_EVENT_H_ */
