#include <pwd.h>
#include <sys/wait.h>

#include "rp_common.h"
#include "rp_config.h"
#include "rp_socket.h"
#include "rp_connection.h"

int main(int argc, char **argv)
{
    pid_t pid;
    int i, tc = 0;
    rp_settings_t s;
    rp_connection_t l = { 0 };
    struct passwd *pw = NULL;
    char c, *ptr, *filename = NULL;
    rp_connection_pool_t srv = { 0 };

    for(i = 1; i < argc; i++) {
        ptr = argv[i];
        if(*ptr == '-' && (c = *++ptr) != '\0' && *++ptr == '\0') {
            switch(c) {
                case 't':
                    tc = 1;
                case 'c':
                    if(i + 1 < argc) {
                        filename = argv[++i];
                        continue;
                    }
                    break;
            }
        }
        fprintf(stderr, "Usage: %s -[ct] redis-proxy.conf\n", argv[0]);
        return EXIT_FAILURE;
    }

    l.settings.auth.data = NULL;
    l.settings.auth.length = RP_NULL_STRLEN;
    l.settings.address.data = "0.0.0.0";
    l.settings.address.length = sizeof(l.settings.address.data) - 1;
    l.settings.port = RP_DEFAULT_PORT;
    l.address = INADDR_ANY;
    l.port = htons(RP_DEFAULT_PORT);
    l.sockfd = -1;

    s.listen = &l;
    s.servers = &srv;

    if(filename != NULL) {
        if(rp_config_file_parse(filename, &s) != RP_SUCCESS) {
            return EXIT_FAILURE;
        }
        if(tc) {
            printf("%s: Syntax OK\n", filename);
            return EXIT_SUCCESS;
        }
    }
    if(!srv.size) {
        fprintf(stderr, "at least one server must be specified\n");
        return EXIT_FAILURE;
    }
    if((pid = fork()) != 0) {
        if(pid > 0) {
            return EXIT_SUCCESS;
        }
        fprintf(stderr, "fork at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return EXIT_FAILURE;
    }
    if(!getuid()) {
        pw = getpwnam("nobody");
    }
    setsid();
    openlog(NULL, LOG_PID, LOG_USER);
    if(rp_listen(&l.sockfd, l.address, l.port) == NULL) {
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "The proxy is now ready to accept connections on %s:%d",
        l.settings.address.data, l.settings.port);
    for(;;) {
        if((pid = fork()) < 0) {
            syslog(LOG_ERR, "fork at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
            return EXIT_FAILURE;
        } else if(pid > 0) {
            while(wait(&i) > 0);
        } else {
            setsid();
            if(pw != NULL && setuid(pw->pw_uid) != 0) {
                syslog(LOG_ERR, "setuid at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
            }
            return rp_connection_handler_loop(&l, &srv);
        }
    }
    return EXIT_SUCCESS;
}
