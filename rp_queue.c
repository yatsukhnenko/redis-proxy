#include "rp_queue.h"

rp_queue_t *rp_queue_init(rp_queue_t *queue, size_t size)
{
    int alloc = 0;
    if(queue == NULL) {
        if((queue = malloc(sizeof(rp_queue_t))) == NULL) {
            syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
            return NULL;
        }
        alloc = 1;
    }
    if((queue->data = calloc(size, sizeof(void *))) == NULL) {
        syslog(LOG_ERR, "calloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        if(alloc) {
            free(queue);
        }
        return NULL;
    }
    memset(queue->data, 0, size * sizeof(void *));
    queue->current = queue->last = 0;
    queue->size = size;
    return queue;
}

void rp_queue_push(rp_queue_t *queue, void *ptr)
{
    queue->data[queue->last++] = ptr;
    if(queue->last == queue->size) {
        queue->last = 0;
    }
}


void *rp_queue_shift(rp_queue_t *queue)
{
    void *ptr = NULL;

    if(queue->current != queue->last) {
        ptr = queue->data[queue->current];
        queue->data[queue->current++] = NULL;
        if(queue->current == queue->size) {
            queue->current = 0;
        }
    }
    return ptr;
}
