#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <sys/types.h>

/*
 * Shared socket boilerplate. This is the only code reused across phases,
 * because it is identical everywhere and is not the thing being taught.
 * The interesting part of each phase (how it waits for I/O) lives in that
 * phase's own server.c, never here.
 */

#define PORT        9999
#define BACKLOG     16
#define BUF_SIZE    1024
#define MAX_CLIENTS 128

/* Create a listening TCP socket bound to PORT (IPv4, SO_REUSEADDR).
   Returns the fd, or -1 on error. */
int create_listener(void);

/* Put fd into non-blocking mode. Returns 0, or -1 on error. */
int set_non_blocking(int fd);

/* Write the whole buffer, looping over short writes and retrying EINTR.
   Returns len on success, or -1 on error. */
ssize_t write_all(int fd, const char *buf, size_t len);

#endif
