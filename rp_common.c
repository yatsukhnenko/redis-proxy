#include "rp_common.h"

int rp_resize_buffer(rp_buffer_t *buffer, int delta)
{
    char *ptr;

    if((ptr = realloc(buffer->s.data, buffer->s.length + delta)) == NULL) {
        syslog(LOG_ERR, "realloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    buffer->s.length += delta;
    buffer->s.data = ptr;

    return RP_SUCCESS;
}
