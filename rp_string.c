#include "rp_string.h"

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
    char sign = 1;
    long int i = 0;

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
