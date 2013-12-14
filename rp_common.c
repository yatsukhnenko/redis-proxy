#include "rp_common.h"

int rp_buffer_resize(rp_buffer_t *buf, int delta)
{
    char *ptr;
    size_t size;

    size = buf->s.length < 0 ? delta : buf->s.length + delta;
    if((ptr = realloc(buf->s.data, size)) == NULL) {
        syslog(LOG_ERR, "realloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    buf->s.length = size;
    buf->s.data = ptr;
    return RP_SUCCESS;
}
