#include "rp_event.h"

#if defined(RP_HAVE_EPOLL)
#include "rp_epoll.h"
#elif defined(RP_HAVE_KQUEUE)
#include "rp_kqueue.h"
#else
#include "rp_select.h"
#endif

rp_event_handler_t *rp_event_handler_init(rp_event_handler_t *eh, size_t maxevents)
{
    if((eh->events = calloc(maxevents, sizeof(rp_event_t))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    memset(eh->events, 0, maxevents * sizeof(rp_event_t));
    if((eh->ready = calloc(maxevents, sizeof(rp_event_t))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(eh->events);
        return NULL;
    }

#if defined(RP_HAVE_EPOLL)
    if(rp_epoll_init(eh, maxevents) == NULL) {
#elif defined(RP_HAVE_KQUEUE)
    if(rp_kqueue_init(eh, maxevents) == NULL) {
#else
    if(rp_select_init(eh, maxevents) == NULL) {
#endif
        free(eh->ready);
        free(eh->events);
        return NULL;
    }

    eh->maxevents = maxevents;
    return eh;
}
