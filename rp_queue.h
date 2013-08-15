#ifndef _RP_QUEUE_H_
#define _RP_QUEUE_H_

#include "rp_common.h"

typedef struct {
    void **data;
    size_t size;
    unsigned int current;
    unsigned int last;
} rp_queue_t;

rp_queue_t *rp_queue_init(rp_queue_t *queue, size_t size);
void rp_queue_push(rp_queue_t *queue, void *ptr);
void *rp_queue_shift(rp_queue_t *queue);

#endif /* _RP_QUEUE_H_ */
