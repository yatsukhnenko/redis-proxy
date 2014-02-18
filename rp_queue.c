#include "rp_queue.h"

int rp_queue_push(rp_queue_t *queue, void *data)
{
    rp_queue_element_t *qe;

    if((qe = malloc(sizeof(rp_queue_element_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return RP_FAILURE;
    }
    if(queue->size) {
        qe->prev = queue->last;
        queue->last->next = qe;
    } else {
        qe->next = qe->prev = NULL;
        queue->first = qe;
    }
    qe->data = data;
    qe->next = NULL;
    queue->last = qe;
    queue->size++;
    return RP_SUCCESS;
}


void *rp_queue_shift(rp_queue_t *queue)
{
    void *data;
    rp_queue_element_t *qe;

    if((qe = queue->first) != NULL) {
        if(--queue->size) {
            queue->first = qe->next;
            queue->first->prev = NULL;
        } else {
            queue->first = queue->last = NULL;
        }
        data = qe->data;
        free(qe);
        return data;
    }
    return NULL;
}
