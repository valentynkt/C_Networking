#include "common.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * select() multiplexed echo server.
 *
 * Instead of spinning, we hand the kernel a set of fds and call select(),
 * which sleeps until at least one of them is ready to read. An idle server
 * now uses almost no CPU. This is the fix for phase 02's busy loop.
 *
 * select() carries two scars from the 1980s, though:
 *   - The fd set is a fixed-size bitmask capped at FD_SETSIZE (1024 here).
 *     A connection numbered above that simply cannot be watched.
 *   - select() clobbers the set it is given, so you rebuild it every loop,
 *     and afterwards you scan every fd to find which ones are ready: O(n)
 *     work on each wakeup whether 1 client or 1000 are active.
 * poll() and then kqueue chip away at both.
 */

typedef struct {
    int     server_fd;
    int     max_fd;
    bool    is_client[FD_SETSIZE];
    fd_set  active_fds;
} server_t;

static void recompute_max_fd(server_t *s)
{
    for (int fd = s->max_fd; fd >= 0; fd--) {
        if (FD_ISSET(fd, &s->active_fds)) {
            s->max_fd = fd;
            return;
        }
    }
}

static void remove_client(server_t *s, int fd)
{
    printf("client disconnected (fd=%d)\n", fd);
    close(fd);
    s->is_client[fd] = false;
    FD_CLR(fd, &s->active_fds);
    if (fd == s->max_fd)
        recompute_max_fd(s);
}

static void accept_client(server_t *s)
{
    int fd = accept(s->server_fd, NULL, NULL);
    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return;
    }
    if (fd >= FD_SETSIZE) {
        fprintf(stderr, "fd %d >= FD_SETSIZE, rejecting\n", fd);
        close(fd);
        return;
    }
    if (set_non_blocking(fd) == -1) {
        close(fd);
        return;
    }
    s->is_client[fd] = true;
    FD_SET(fd, &s->active_fds);
    if (fd > s->max_fd)
        s->max_fd = fd;
    printf("client connected (fd=%d)\n", fd);
}

static void handle_client(server_t *s, int fd)
{
    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n == 0) {
        remove_client(s, fd);
        return;
    }
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        perror("read");
        remove_client(s, fd);
        return;
    }
    if (write_all(fd, buf, (size_t)n) == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write");
            remove_client(s, fd);
        }
    }
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

    printf("[select] listening on port %d\n", PORT);

    server_t s;
    s.server_fd = server_fd;
    s.max_fd = server_fd;
    memset(s.is_client, 0, sizeof(s.is_client));
    FD_ZERO(&s.active_fds);
    FD_SET(server_fd, &s.active_fds);

    for (;;) {
        /* select() overwrites the set, so hand it a fresh copy each loop. */
        fd_set readfds = s.active_fds;
        int ready = select(s.max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        if (FD_ISSET(s.server_fd, &readfds))
            accept_client(&s);

        for (int fd = 0; fd <= s.max_fd; fd++) {
            if (s.is_client[fd] && FD_ISSET(fd, &readfds))
                handle_client(&s, fd);
        }
    }

    for (int fd = 0; fd <= s.max_fd; fd++) {
        if (s.is_client[fd])
            close(fd);
    }
    close(server_fd);
    return 1;
}
