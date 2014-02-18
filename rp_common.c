#include "rp_common.h"

int rp_buffer_resize(rp_buffer_t *buffer, int delta)
{
    char *data;
    size_t size;

    size = buffer->s.length < 0 ? delta : buffer->s.length + delta;
    if((data = realloc(buffer->s.data, size)) == NULL) {
        syslog(LOG_ERR, "realloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    buffer->s.length = size;
    buffer->s.data = data;
    return RP_SUCCESS;
}

int rp_buffer_append(rp_buffer_t *buffer, rp_string_t *s)
{
    if(s->length > 0) {
        if((int)buffer->used + s->length > buffer->s.length) {
            if(rp_buffer_resize(buffer, (s->length / RP_BUFFER_SIZE + 1) * RP_BUFFER_SIZE) != RP_SUCCESS) {
                return RP_FAILURE;
            }
        }
        memcpy(&buffer->s.data[buffer->used], s->data, s->length);
        buffer->used += s->length;
    }
    return RP_SUCCESS;
}
