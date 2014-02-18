#include "rp_string.h"

rp_string_t *rp_string(const char *format, ...)
{
    size_t l;
    va_list ap;
    char *data, *ptr, *str;
    rp_string_t *arg, *s = NULL;

    if((s = (rp_string_t *)malloc(sizeof(rp_string_t))) == NULL) {
        syslog(LOG_ERR, "malloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    s->data = NULL;
    if(format == NULL) {
        s->length = RP_NULL_STRLEN;
        return s;
    }
    s->length = 0;
    str = (char *)format;
    if((ptr = strchr(str, '%')) != NULL) {
        va_start(ap, format);
        do {
            arg = NULL;
            l = ptr++ - str;
            if(*ptr == 's') {
                arg = va_arg(ap, rp_string_t *);
            } else {
                l++;
            }
            if((data = realloc(s->data, s->length + l + (arg != NULL ? arg->length : 0))) == NULL) {
                syslog(LOG_ERR, "realloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
                free(s->data);
                free(s);
                return NULL;
            }
            s->data = data;
            memcpy(&s->data[s->length], str, l);
            s->length += l;
            if(arg != NULL) {
                memcpy(&s->data[s->length], arg->data, arg->length);
                s->length += arg->length;
            }
            str = ++ptr;
        } while((ptr = strchr(str, '%')) != NULL);
        va_end(ap);
    }
    l = strlen(str);
    if((data = realloc(s->data, s->length + l)) == NULL) {
        syslog(LOG_ERR, "realloc at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        free(s->data);
        free(s);
        return NULL;
    }
    s->data = data;
    memcpy(&s->data[s->length], str, l);
    s->length += l;
    return s;
}

char *rp_strstr(rp_string_t *s, const char *needle)
{
    char *ptr, *str;
    size_t i = 0;

    while(i < s->length) {
        if(s->data[i] != *needle) {
            i++;
            continue;
        }
        ptr = &s->data[i];
        str = (char *)needle;
        while(*str != '\0' && s->data[++i] == *++str);
        if(*str == '\0') {
            return ptr;
        }
    }
    return NULL;
}


long int rp_strtol(rp_string_t *s)
{
    long i = 0;
    int sign = 1;

    while(s->length > 0) {
        if(*s->data == '-') {
            if(sign > 0) {
                sign = -1;
                s->data++;
                s->length--;
                continue;
            }
        } else if(*s->data >= '0' && *s->data <= '9') {
            i = i * 10 + *s->data - '0';
            s->data++;
            s->length--;
            continue;
        }
        break;
    }
    return i * sign;
}
