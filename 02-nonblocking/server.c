#include "common.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Non-blocking busy-poll echo server.
 *
 * Every fd is put into O_NONBLOCK mode, so accept() and read() return
 * immediately with EAGAIN instead of sleeping when there is nothing to
 * do. One flat loop tries to accept, then walks every known client and
 * tries to read. Now many clients are served at once, no single one can
 * freeze the others.
 *
 * The catch: the loop never sleeps. When all clients are idle it still
 * spins as fast as the CPU allows, burning a whole core on EAGAIN. Run
 * `top` next to an idle server and watch it sit at 100%. That waste is
 * what select() fixes.
 */

typedef struct {
    int server_fd;
    int clients[MAX_CLIENTS];
    int count;
} server_t;

static void remove_client(server_t *s, int idx)
{
    printf("client disconnected (fd=%d)\n", s->clients[idx]);
    close(s->clients[idx]);
    s->clients[idx] = s->clients[s->count - 1];
    s->count--;
}

static void accept_client(server_t *s)
{
    int fd = accept(s->server_fd, NULL, NULL);
    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return;
    }
    if (s->count >= MAX_CLIENTS || set_non_blocking(fd) == -1) {
        close(fd);
        return;
    }
    s->clients[s->count++] = fd;
    printf("client connected (fd=%d)\n", fd);
}

/* Returns true if the client was removed (so the caller can re-check the
   index that the last client was swapped into). */
static bool handle_client(server_t *s, int idx)
{
    int fd = s->clients[idx];
    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n == 0) {
        remove_client(s, idx);
        return true;
    }
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return false; /* nothing ready, normal for non-blocking */
        perror("read");
        remove_client(s, idx);
        return true;
    }
    if (write_all(fd, buf, (size_t)n) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;
        perror("write");
        remove_client(s, idx);
        return true;
    }
    return false;
}

int main(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return 1;
    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return 1;
    }

    printf("[nonblocking] listening on port %d\n", PORT);

    server_t s = { .server_fd = server_fd, .count = 0 };

    for (;;) {
        accept_client(&s);
        for (int i = 0; i < s.count; i++) {
            if (handle_client(&s, i))
                i--; /* a client was swapped into i, re-check it */
        }
    }
}
