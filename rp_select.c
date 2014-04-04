#include "rp_connection.h"

#ifdef RP_HAVE_SELECT
#include "rp_select.h"

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
    eh->poll = rp_select_wait;
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

void rp_select_wait(struct rp_event_handler *eh, struct timeval *timeout)
{
    int i, j, n;
    rp_event_t e;
    fd_set rfds, wfds;
    struct timeval tv;
    rp_connection_t *c;
    rp_select_data_t *sd = eh->data;

    rfds = sd->rfds;
    wfds = sd->wfds;
    if((n = select(sd->nfds + 1, &rfds, &wfds, NULL, timeout)) < 0) {
        syslog(LOG_ERR, "select at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
    } else if(n > 0) {
        j = 0;
        gettimeofday(&tv, NULL);
        for(i = 0; i < sd->nfds + 1 && j < n; i++) {
            e.events = 0;
            if(FD_ISSET(i, &rfds)) {
                e.events |= RP_EVENT_READ;
                j++;
            }
            if(FD_ISSET(i, &wfds)) {
                e.events |= RP_EVENT_WRITE;
                j++;
            }
            if(e.events) {
                c = eh->events[i].data;
                c->time = tv.tv_sec + tv.tv_usec / 1000000.0;
                if((!(e.events & RP_EVENT_READ) || c->on.read(c, eh) == RP_SUCCESS)
                    && (!(e.events & RP_EVENT_WRITE) || c->on.write(c, eh) == RP_SUCCESS)) {
                    continue;
                }
                c->on.close(c, eh);
            }
        }
    }
}

#endif /* RP_HAVE_SELECT */
