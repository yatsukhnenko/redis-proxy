#include "rp_select.h"
#ifdef RP_HAVE_SELECT

rp_event_handler_t *rp_select_init(rp_event_handler_t *eh)
{
    rp_select_data_t *sd = eh->data;

    if((sd = malloc(sizeof(rp_select_data_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    sd->nfds = -1;
    FD_ZERO(&sd->rfds);
    FD_ZERO(&sd->wfds);
    eh->data = sd;
    eh->add = rp_select_add;
    eh->del = rp_select_del;
    eh->wait = rp_select_wait;
    syslog(LOG_INFO, "using 'select' I/O multiplexing mechanism");
    return eh;
}


int rp_select_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e)
{
    rp_select_data_t *sd = eh->data;

    if(e->events & RP_EVENT_READ) {
        FD_SET(sockfd, &sd->rfds);
    }
    if(e->events & RP_EVENT_WRITE) {
        FD_SET(sockfd, &sd->wfds);
    }
    eh->events[sockfd].events |= e->events;
    eh->events[sockfd].data = e->data;
    if(sockfd > sd->nfds) {
        sd->nfds = sockfd;
    }
    return RP_SUCCESS;
}

int rp_select_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e)
{
    int i;
    rp_select_data_t *sd = eh->data;

    if(e->events & RP_EVENT_READ) {
        FD_CLR(sockfd, &sd->rfds);
    }
    if(e->events & RP_EVENT_WRITE) {
        FD_CLR(sockfd, &sd->wfds);
    }
    eh->events[sockfd].events &= ~e->events;
    if(!eh->events[sockfd].events) {
        eh->events[sockfd].data = NULL;
        if(sockfd == sd->nfds) {
            sd->nfds = -1;
            for(i = sockfd - 1; i >= 0; i--) {
                if(eh->events[i].events) {
                    sd->nfds = i;
                    break;
                }
            }
        }
    }
    return RP_SUCCESS;
}

int rp_select_wait(struct rp_event_handler *eh, struct timeval *timeout)
{
    rp_event_t e;
    int i, j, ready;
    fd_set rfds, wfds;
    rp_select_data_t *sd = eh->data;

    rfds = sd->rfds;
    wfds = sd->wfds;
    if((ready = select(sd->nfds + 1, &rfds, &wfds, NULL, timeout)) < 0) {
        syslog(LOG_ERR, "select at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
    } else if(ready > 0) {
        j = 0;
        for(i = 0; i < sd->nfds + 1; i++) {
            e.events = 0;
            if(FD_ISSET(i, &rfds)) {
                e.events |= RP_EVENT_READ;
            }
            if(FD_ISSET(i, &wfds)) {
                e.events |= RP_EVENT_WRITE;
            }
            if(e.events) {
                eh->ready[j].events = e.events;
                eh->ready[j].data = eh->events[i].data;
                if(++j == ready) {
                    break;
                }
            }
        }
    }
    return ready;
}

#endif /* RP_HAVE_SELECT */
