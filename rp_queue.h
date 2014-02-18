#ifndef _RP_QUEUE_H_
#define _RP_QUEUE_H_

#include "rp_common.h"

typedef struct rp_queue_element {
    void *data;
    struct rp_queue_element *prev;
    struct rp_queue_element *next;
} rp_queue_element_t;

typedef struct {
    size_t size;
    rp_queue_element_t *first;
    rp_queue_element_t *last;
} rp_queue_t;

int rp_queue_push(rp_queue_t *queue, void *data);
void *rp_queue_shift(rp_queue_t *queue);

#endif /* _RP_QUEUE_H_ */
