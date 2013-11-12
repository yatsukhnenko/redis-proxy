#include <pwd.h>
#include <sys/wait.h>

#include "rp_common.h"
#include "rp_config.h"
#include "rp_socket.h"
#include "rp_connection.h"

int main(int argc, char **argv)
{
    int i;
    pid_t pid;
    rp_settings_t s;
    rp_connection_t l = { 0 };
    struct passwd *pw = NULL;
    char c, *ptr, *filename = NULL;
    rp_connection_pool_t pool = { 0 };
    struct in_addr addr = { INADDR_ANY };

    for(i = 1; i < argc; i++) {
        ptr = argv[i];
        if(*ptr == '-' && (c = *++ptr) != '\0' && *++ptr == '\0') {
            switch(c) {
                case 'c':
                    if(i + 1 < argc) {
                        filename = argv[++i];
                        continue;
                    }
                    break;
            }
        }
        fprintf(stderr, "Usage: %s -c redis-proxy.conf\n", argv[0]);
        return EXIT_FAILURE;
    }

    l.sockfd = -1;
    l.address = addr.s_addr;
    l.hr.address.data = "0.0.0.0";
    l.hr.address.length = strlen(l.hr.address.data);
    l.port = htons(RP_DEFAULT_PORT);
    l.hr.port = RP_DEFAULT_PORT;

    s.listen = &l;
    s.servers = &pool;

    openlog(NULL, LOG_PID, LOG_USER);

    if(filename != NULL) {
        if(rp_config_file_parse(filename, &s) != RP_SUCCESS) {
            return EXIT_FAILURE;
        }
    }
    if(!pool.size) {
        syslog(LOG_ERR, "at least one server must be specified\n");
        return EXIT_FAILURE;
    }
    if((pid = fork()) != 0) {
        if(pid > 0) {
            return EXIT_SUCCESS;
        }
        syslog(LOG_ERR, "fork at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
        return EXIT_FAILURE;
    }
    if(!getuid()) {
        pw = getpwnam("nobody");
    }
    setsid();
    if(rp_listen(&l.sockfd, l.address, l.port) == NULL) {
        return EXIT_FAILURE;
    }
    syslog(LOG_INFO, "The proxy is now ready to accept connections on %s:%d", l.hr.address.data, l.hr.port);
    for(;;) {
        if((pid = fork()) < 0) {
            syslog(LOG_ERR, "fork at %s:%d - %s\n", __FILE__, __LINE__, strerror(errno));
            return EXIT_FAILURE;
        } else if(pid > 0) {
            while(wait(&i) > 0);
        } else {
            setsid();
            if(pw != NULL) {
                setuid(pw->pw_uid);
            }
            return rp_connection_handler_loop(&l, &pool);
        }
    }
    return EXIT_SUCCESS;
}
