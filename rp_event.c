#include "rp_connection.h"

#if defined(RP_HAVE_EPOLL)
#include "rp_epoll.h"
#elif defined(RP_HAVE_KQUEUE)
#include "rp_kqueue.h"
#else
#include "rp_select.h"
#endif

rp_event_handler_t *rp_event_handler_init(rp_event_handler_t *eh)
{
#ifdef RP_HAVE_SELECT
    eh->maxevents = FD_SETSIZE;
#else
    eh->maxevents = RP_BUFFER_SIZE;
#endif
    if((eh->events = calloc(eh->maxevents, sizeof(rp_event_t))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    memset(eh->events, 0, eh->maxevents * sizeof(rp_event_t));
#if defined(RP_HAVE_EPOLL)
    if(rp_epoll_init(eh) == NULL) {
#elif defined(RP_HAVE_KQUEUE)
    if(rp_kqueue_init(eh) == NULL) {
#else
    if(rp_select_init(eh) == NULL) {
#endif
        free(eh->events);
        return NULL;
    }
    return eh;
}
