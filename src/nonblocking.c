#include "nonblocking.h"
#include "common.h"
#include <stdbool.h>

/*
 * Phase 2: Non-Blocking Busy-Poll Echo Server
 *
 * All fds set to O_NONBLOCK. Single flat loop: try accept, then
 * iterate all tracked clients. EAGAIN means "nothing ready, skip."
 *
 * Multiple clients work simultaneously. But CPU burns at 100%
 * even when idle — millions of EAGAIN returns per second.
 * This motivates select() and kqueue.
 *
 * Feel the problem: run `top` while the server is idle.
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
    if (set_non_blocking(fd) == -1) {
        close(fd);
        return;
    }
    if (s->count >= MAX_CLIENTS) {
        close(fd);
        return;
    }
    s->clients[s->count++] = fd;
    printf("client connected (fd=%d)\n", fd);
}

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
            return false;
        perror("read");
        remove_client(s, idx);
        return true;
    }

    char response[BUF_SIZE + PREFIX_LEN + SUFFIX_LEN];
    size_t resp_len = format_response(response, buf, (size_t)n);
    if (write_all(fd, response, resp_len) == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return false;
        perror("write");
        remove_client(s, idx);
        return true;
    }
    return false;
}

int run_nonblocking_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[nonblocking] listening on port %d\n", PORT);

    server_t s = { .server_fd = server_fd, .count = 0 };

    for (;;) {
        accept_client(&s);

        for (int i = 0; i < s.count; i++) {
            if (handle_client(&s, i))
                i--;
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
