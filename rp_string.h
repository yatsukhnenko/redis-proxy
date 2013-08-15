#ifndef _RP_STRING_H
#define _RP_STRING_H_

#include <stdlib.h>

typedef struct {
    char *data;
    size_t length;
} rp_string_t;

char *rp_strstr(rp_string_t *s, const char *needle);

#endif /* _RP_STRING_H_ */
