#include "rp_socket.h"

int *rp_socket_init(int *sockfd)
{
    int yes = 1;

#ifdef RP_LINUX
    if((*sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
        syslog(LOG_ERR, "socket at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
#else
    if((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        syslog(LOG_ERR, "socket at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        return NULL;
    }
    if(fcntl(*sockfd, F_SETFD, O_NONBLOCK) < 0) {
        syslog(LOG_ERR, "fcntl at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(*sockfd);
        return NULL;
    }
#endif
    if(setsockopt(*sockfd, SOL_SOCKET, SO_KEEPALIVE, &yes, sizeof(yes)) < 0) {
        syslog(LOG_ERR, "setsockopt at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(*sockfd);
        return NULL;
    }
    return sockfd;
}

int *rp_listen(int *sockfd, in_addr_t address, in_port_t port)
{
    int s, yes = 1;
    struct sockaddr_in addr;

    if(rp_socket_init(&s) == NULL) {
        return NULL;
    }
    if(setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        syslog(LOG_ERR, "setsockopt at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(s);
        return NULL;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = address;
    addr.sin_port = port;
    if(bind(s, (const struct sockaddr *)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "bind at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(s);
        return NULL;
    }
    if(listen(s, RP_CONCURRENT_CONNECTIONS) < 0) {
        syslog(LOG_ERR, "listen at %s:%d - %s", __FILE__, __LINE__, strerror(errno));
        close(s);
        return NULL;
    }
    *sockfd = s;
    return sockfd;
}

int rp_recv(int sockfd, rp_buffer_t *buffer)
{
    ssize_t i;

    for(;;) {
        if((int)buffer->used + RP_BUFFER_SIZE > buffer->s.length) {
            if(rp_buffer_resize(buffer, RP_BUFFER_SIZE) != RP_SUCCESS) {
                break;
            }
        }
        if((i = recv(sockfd, &buffer->s.data[buffer->used],
            buffer->s.length - buffer->used - 1, MSG_DONTWAIT)) <= 0) {
            if(i && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            return RP_FAILURE;
        }
        buffer->used += i;
    }
    buffer->s.data[buffer->used] = '\0';
    return RP_SUCCESS;
}


int rp_send(int sockfd, rp_buffer_t *buffer)
{
    ssize_t i;

    while(buffer->w < (int)buffer->used) {
        if((i = send(sockfd, &buffer->s.data[buffer->w],
            buffer->used - buffer->w, MSG_DONTWAIT | MSG_NOSIGNAL)) <= 0) {
            if(i && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                break;
            }
            return RP_FAILURE;
        }
        buffer->w += i;
    }
    return RP_SUCCESS;
}
