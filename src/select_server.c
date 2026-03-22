#include "select_server.h"
#include "common.h"
#include <stdbool.h>

/*
 * Phase 3: select() Multiplexed Echo Server
 *
 * The kernel does the waiting — select() blocks until at least one fd
 * is ready. CPU drops to ~0% when idle. This fixes Phase 2's busy-loop.
 *
 * Key difference from Phase 2:
 *   Phase 2: you poll every fd yourself, millions of EAGAIN per second
 *   Phase 3: kernel tells you which fds have data, you only check those
 */

typedef struct {
    int server_fd;
    int max_fd;
    bool is_client[FD_SETSIZE];
    fd_set active_fds;
} server_t;

static void init_server(server_t *s, int server_fd)
{
    s->server_fd = server_fd;
    s->max_fd = server_fd;
    memset(s->is_client, 0, sizeof(s->is_client));
    FD_ZERO(&s->active_fds);
    FD_SET(server_fd, &s->active_fds);
}

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

    char response[BUF_SIZE + PREFIX_LEN + SUFFIX_LEN];
    size_t resp_len = format_response(response, buf, (size_t)n);
    if (write_all(fd, response, resp_len) == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write");
            remove_client(s, fd);
        }
    }
}

static void shutdown_server(server_t *s)
{
    for (int fd = 0; fd <= s->max_fd; fd++) {
        if (s->is_client[fd])
            close(fd);
    }
    close(s->server_fd);
}

int run_select_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[select] listening on port %d\n", PORT);

    server_t s;
    init_server(&s, server_fd);

    for (;;) {
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

    shutdown_server(&s);
    return EXIT_FAILURE;
}
