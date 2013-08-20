#include "rp_kqueue.h"

rp_event_handler_t *rp_kqueue_init(rp_event_handler_t *eh, size_t maxevents)
{
    rp_kqueue_data_t *kd = NULL;

    if((kd = malloc(sizeof(rp_kqueue_data_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    if((kd->events = calloc(maxevents, sizeof(struct kevent))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(kd);
        return NULL;
    }
    if((kd->kqfd = kqueue()) < 0) {
        syslog(LOG_ERR, "kqueue at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(kd->events);
        free(kd);
        return NULL;
    }
    eh->data = kd;
    eh->add = rp_kqueue_add;
    eh->del = rp_kqueue_del;
    eh->wait = rp_kqueue_wait;

    return eh;
}

int rp_kqueue_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e)
{
    int i = 0;
    struct kevent event[2];
    rp_kqueue_data_t *kd = eh->data;

    if(e->events & RP_EVENT_READ) {
        EV_SET(&event[i++], sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    }
    if(e->events & RP_EVENT_WRITE) {
        EV_SET(&event[i++], sockfd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
    }
    if(kevent(kd->kqfd, event, i, NULL, 0, NULL) < 0) {
        syslog(LOG_ERR, "kevent at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    eh->events[sockfd].events |= e->events;
    eh->events[sockfd].data = e->data;

    return RP_SUCCESS;
}

int rp_kqueue_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e)
{
    int i = 0;
    struct kevent event[2];
    rp_kqueue_data_t *kd = eh->data;

    if(e->events & RP_EVENT_READ) {
        EV_SET(&event[i++], sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    }
    if(e->events & RP_EVENT_WRITE) {
        EV_SET(&event[i++], sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    }
    if(kevent(kd->kqfd, event, i, NULL, 0, NULL) < 0) {
        syslog(LOG_ERR, "kevent at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    eh->events[sockfd].events &= ~e->events;
    if(!eh->events[sockfd].events) {
        eh->events[sockfd].data = NULL;
    }

    return RP_SUCCESS;
}

int rp_kqueue_wait(struct rp_event_handler *eh, struct timeval *timeout)
{
    int i, ready;
    struct kevent *event;
    struct timespec ts, *pts;
    rp_kqueue_data_t *kd = eh->data;

    pts = NULL;
    if(timeout) {
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000;
        pts = &ts;
    }

    if((ready = kevent(kd->kqfd, NULL, 0, kd->events, eh->maxevents, pts)) < 0) {
        syslog(LOG_ERR, "kevent at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
    } else if(ready > 0) {
        for(i = 0; i < ready; i++) {
            event = &kd->events[i];
            eh->ready[i].events = 0;
            eh->ready[i].data = eh->events[event->ident].data;
            if(event->filter == EVFILT_READ) {
                eh->ready[i].events |= RP_EVENT_READ;
            }
            if(event->filter == EVFILT_WRITE) {
                eh->ready[i].events |= RP_EVENT_WRITE;
            }
        }
    }

    return ready;
}
