# Linux only. epoll has no macOS equivalent header, so this will not build
# on a Mac. Use a Linux machine or a container.
CC     = gcc
CFLAGS = -Wall -Wextra -Werror -std=c17 -g -fsanitize=address,undefined

server: server.c ../common/common.c
	$(CC) $(CFLAGS) -I../common -o server server.c ../common/common.c

clean:
	rm -f server
	rm -rf server.dSYM

.PHONY: clean
