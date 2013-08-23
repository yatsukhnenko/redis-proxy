CC=gcc
CFLAGS=-pipe -O2 -Wall -pedantic -ansi
OBJ=rp_string.o rp_queue.o rp_socket.o rp_select.o rp_epoll.o rp_kqueue.o rp_event.o rp_redis.o rp_connection.o

.PHONY: all clean

all: $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) rp_main.c -o redis-proxy
clean:
	rm -f *.o
	rm -f redis-proxy
