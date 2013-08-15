#include <pwd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "rp_common.h"
#include "rp_socket.h"
#include "rp_connection.h"

int *rp_parse_port(int *port, char *str);

int main(int argc, char **argv)
{
    pid_t pid;
    int i, port;
    char c, *ptr;
    struct passwd *pw;
    struct hostent *he;
    rp_connection_t l = {0};
    rp_connection_pool_t s = {0, 0, NULL};

    l.sockfd = -1;
    l.address = INADDR_ANY;
    l.port = 0xEB18;
    strcpy(l.hr.address, "0.0.0.0");
    l.hr.port = RP_DEFAULT_PORT;

    for(i = 1; i < argc; i++) {
        ptr = argv[i];
        if(*ptr == '-' && (c = *++ptr) != '\0' && *++ptr == '\0') {
            switch(c) {
                case 'L':
                    if(i < argc - 1) {
                        if((ptr = strchr(argv[++i], ':')) != NULL) {
                            *ptr++ = '\0';
                            if(!inet_pton(AF_INET, argv[i], &l.address)) {
                                fprintf(stderr, "Error: invalid address - '%s'\n", argv[i]);
                                return EXIT_FAILURE;
                            }
                        } else {
                            ptr = argv[i];
                        }
                        if(rp_parse_port(&port, ptr) == NULL) {
                            fprintf(stderr, "Error: invalid port number '%s'\n", ptr);
                            return EXIT_FAILURE;
                        }
                        l.hr.port = port;
                        l.port = htons(port);
                        continue;
                    }
                    break;
                case 's':
                    if(i < argc - 1) {
                        port = RP_DEFAULT_PORT;
                        if((ptr = strchr(argv[++i], ':')) != NULL) {
                            *ptr++ = '\0';
                            if(rp_parse_port(&port, ptr) == NULL) {
                                fprintf(stderr, "Error: invalid port number at %s:%d - '%s'\n", __FILE__, __LINE__, ptr);
                                return EXIT_FAILURE;
                            }
                        }
                        if((he = gethostbyname(argv[i])) == NULL) {
                            fprintf(stderr, "Error: gethostbyname at %s:%d\n", __FILE__, __LINE__);
                            return EXIT_FAILURE;
                        }
                        if((s.c = realloc(s.c, ++s.size * sizeof(rp_connection_t))) == NULL) {
                            fprintf(stderr, "Error: realloc at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
                            return EXIT_FAILURE;
                        }
                        s.c[s.size - 1].sockfd = -1;
                        s.c[s.size - 1].address = ((struct in_addr *)(he->h_addr_list[0]))->s_addr;
                        s.c[s.size - 1].port = htons(port);
                        strcpy(s.c[s.size - 1].hr.address, inet_ntoa(*(struct in_addr *)(he->h_addr_list[0])));
                        s.c[s.size - 1].hr.port = port;
                        s.c[s.size - 1].data = NULL;
                        s.c[s.size - 1].flags = 0;
                        s.c[s.size - 1].time = 0;
                        continue;
                    }
                    break;
            }
        }
        fprintf(stderr, "Usage: %s [-L [ip:]port] [-s address[:port] ... ]\n", argv[0]);
        return EXIT_FAILURE;
    }
    if(!s.size) {
        fprintf(stderr, "Error: at least one server must be specified\n");
        return EXIT_FAILURE;
    }
    if((pw = getpwnam("nobody")) == NULL) {
        fprintf(stderr, "Error: getpwnam at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return EXIT_FAILURE;
    }
    if((pid = fork()) != 0) {
        if(pid > 0) {
            return EXIT_SUCCESS;
        }
        fprintf(stderr, "Error: fork at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return EXIT_FAILURE;
    }
    setsid();
    if(rp_listen(&l.sockfd, l.address, l.port) == NULL) {
        return EXIT_FAILURE;
    }
    openlog(NULL, LOG_PID, LOG_USER);
    syslog(LOG_INFO, "The proxy is now ready to accept connections on %s:%d", l.hr.address, l.hr.port);
    for(;;) {
        if((pid = fork()) < 0) {
            syslog(LOG_ERR, "fork at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
            return EXIT_FAILURE;
        } else if(pid > 0) {
            while(wait(&i) > 0);
        } else {
            setsid();
            setuid(pw->pw_uid);
            return rp_connection_handler_loop(&l, &s);
        }
    }
    return EXIT_SUCCESS;
}


int *rp_parse_port(int *port, char *str)
{
    unsigned int i;

    if(*str < '1' || *str > '9') {
        return NULL;
    }
    i = *str++ - '0';
    while(*str >= '0' && *str <= '9') {
        if((i = i * 10 + *str++ - '0') > 65535) {
            return NULL;
        }
    }
    if(*str != '\0') {
        return NULL;
    }
    *port = i;
    return port;
}
