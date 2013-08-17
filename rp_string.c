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
