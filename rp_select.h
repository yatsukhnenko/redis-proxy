#ifndef _RP_SELECT_H_
#define _RP_SELECT_H_

#include <sys/select.h>
#include "rp_event.h"

typedef struct {
    int nfds;
    fd_set rfds;
    fd_set wfds;
} rp_select_data_t;

rp_event_handler_t *rp_select_init(rp_event_handler_t *eh, size_t maxevents);
int rp_select_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
int rp_select_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e);
int rp_select_wait(struct rp_event_handler *eh, struct timeval *timeout);

#endif /* _RP_SELECT_H_ */
