#ifndef _RP_SOCKET_H_
#define _RP_SOCKET_H_

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "rp_common.h"

int *rp_socket_init(int *sockfd);
int *rp_listen(int *sockfd, in_addr_t address, in_port_t port);
int rp_recv(int sockfd, rp_buffer_t *buffer);
int rp_send(int sockfd, rp_buffer_t *buffer);

#endif /* _RP_SOCKET_H_ */
