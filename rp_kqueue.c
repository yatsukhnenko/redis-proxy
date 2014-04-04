#include "rp_connection.h"

#ifdef RP_HAVE_KQUEUE
#include "rp_kqueue.h"

rp_event_handler_t *rp_kqueue_init(rp_event_handler_t *eh)
{
    rp_kqueue_data_t *kd = NULL;

    if((kd = malloc(sizeof(rp_kqueue_data_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    if((kd->events = calloc(eh->maxevents, sizeof(struct kevent))) == NULL) {
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
    eh->poll = rp_kqueue_wait;
    syslog(LOG_INFO, "using 'kqueue' I/O multiplexing mechanism");
    return eh;
}

int rp_kqueue_add(struct rp_event_handler *eh, int sockfd, rp_event_t *e)
{
    int i = 0;
    struct kevent event[2];
    rp_kqueue_data_t *kd = eh->data;

    if(e->events & RP_EVENT_READ) {
        EV_SET(&event[i++], sockfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
        if(eh->events[sockfd].events & RP_EVENT_READ) {
            e->events &= ~RP_EVENT_READ;
        }
    }
    if(e->events & RP_EVENT_WRITE) {
        EV_SET(&event[i++], sockfd, EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if(eh->events[sockfd].events & RP_EVENT_WRITE) {
            e->events &= ~RP_EVENT_WRITE;
        }
    }
    if(e->events) {
        if(kevent(kd->kqfd, event, i, NULL, 0, NULL) < 0) {
            syslog(LOG_ERR, "kevent at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
            return RP_FAILURE;
        }
        eh->events[sockfd].events |= e->events;
        eh->events[sockfd].data = e->data;
    }
    return RP_SUCCESS;
}

int rp_kqueue_del(struct rp_event_handler *eh, int sockfd, rp_event_t *e)
{
    int i = 0;
    struct kevent event[2];
    rp_kqueue_data_t *kd = eh->data;

    e->events &= eh->events[sockfd].events;
    if(e->events) {
        if(eh->events[sockfd].events & RP_EVENT_READ) {
            if(e->events & RP_EVENT_READ) {
                e->events &= ~RP_EVENT_READ;
                EV_SET(&event[i++], sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            } else {
                e->events |= RP_EVENT_READ;
            }
        } else {
            e->events &= ~RP_EVENT_READ;
        }
        if(eh->events[sockfd].events & RP_EVENT_WRITE) {
            if(e->events & RP_EVENT_WRITE) {
                e->events &= ~RP_EVENT_WRITE;
                EV_SET(&event[i++], sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
            } else {
                e->events |= RP_EVENT_WRITE;
            }
        }
        if(i && e->events != eh->events[sockfd].events) {
            if(kevent(kd->kqfd, event, i, NULL, 0, NULL) < 0) {
                syslog(LOG_ERR, "kevent at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
                return RP_FAILURE;
            }
            eh->events[sockfd].events = e->events;
            if(!eh->events[sockfd].events) {
                eh->events[sockfd].data = NULL;
            }
        }
    }
    return RP_SUCCESS;
}

void rp_kqueue_wait(struct rp_event_handler *eh, struct timeval *timeout)
{
    int i, n;
    struct timeval tv;
    rp_connection_t *c;
    struct kevent *event;
    struct timespec ts, *pts;
    rp_kqueue_data_t *kd = eh->data;

    pts = NULL;
    if(timeout) {
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000;
        pts = &ts;
    }

    if((n = kevent(kd->kqfd, NULL, 0, kd->events, eh->maxevents, pts)) < 0) {
        syslog(LOG_ERR, "kevent at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
    } else if(n > 0) {
        gettimeofday(&tv, NULL);
        for(i = 0; i < n; i++) {
            event = &kd->events[i];
            c = eh->events[event->ident].data;
            c->time = tv.tv_sec + tv.tv_usec / 1000000.0;
            if(!(event->flags & EV_ERROR)) {
                if((event->filter != EVFILT_READ || c->on.read(c, eh) == RP_SUCCESS)
                    && (event->filter != EVFILT_WRITE || c->on.write(c, eh) == RP_SUCCESS)) {
                    continue;
                }
            }
            c->on.close(c, eh);
        }
    }
}

#endif /* RP_HAVE_KQUEUE */
