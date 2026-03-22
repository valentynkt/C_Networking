#ifndef COMMON_H
#define COMMON_H

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 9999
#define BACKLOG 5
#define BUF_SIZE 1024
#define MAX_CLIENTS 128
static const char PREFIX[] = "Resp: ";
static const char SUFFIX[] = "[OK]\n";
static const size_t PREFIX_LEN = sizeof(PREFIX) - 1;
static const size_t SUFFIX_LEN = sizeof(SUFFIX) - 1;

ssize_t write_all(int fd, const char *buf, size_t len);
int set_non_blocking(int fd);
int create_listener(void);

#endif
