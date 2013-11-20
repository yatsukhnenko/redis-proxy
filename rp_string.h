#ifndef _RP_STRING_H
#define _RP_STRING_H_

#include <stdlib.h>

#define RP_NULL_LEN -1

typedef struct {
    int length;
    char *data;
} rp_string_t;

char *rp_strstr(rp_string_t *s, const char *needle);

#endif /* _RP_STRING_H_ */
