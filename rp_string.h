#ifndef _RP_STRING_H
#define _RP_STRING_H_

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <syslog.h>

#define RP_NULL_STRLEN -1

typedef struct {
    int length;
    char *data;
} rp_string_t;

long int rp_strtol(rp_string_t *s);
char *rp_strstr(rp_string_t *s, const char *needle);
rp_string_t *rp_sprintf(rp_string_t *s, const char *format, ...);

#endif /* _RP_STRING_H_ */
