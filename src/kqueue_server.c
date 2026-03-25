#include "kqueue_server.h"
#include "common.h"
#include "event_loop.h"
#include <stdbool.h>
#include <stdlib.h>

#define MAX_CLIENTS_FD 1024

/*
 * Phase 4: kqueue Event Loop Echo Server
 *
 * Register fds once. Kernel returns only ready fds.
 * O(ready) not O(total). No FD_SETSIZE limit.
 *
 * Now uses the shared event_loop abstraction.
 */

typedef struct {
    bool is_client[MAX_CLIENTS_FD];
} kq_state_t;

static void remove_client(event_loop_t *el, int fd)
{
    kq_state_t *state = el->ctx;
    printf("client disconnected (fd=%d)\n", fd);
    el_remove(el, fd);
    close(fd);
    state->is_client[fd] = false;
}

static void on_read(event_loop_t *el, int fd)
{
    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n == 0) {
        remove_client(el, fd);
        return;
    }
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        perror("read");
        remove_client(el, fd);
        return;
    }

    char response[BUF_SIZE + PREFIX_LEN + SUFFIX_LEN];
    size_t resp_len = format_response(response, buf, (size_t)n);
    if (write_all(fd, response, resp_len) == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write");
            remove_client(el, fd);
        }
    }
}
static void on_accept(event_loop_t *el, int server_fd)
{
    kq_state_t *state = el->ctx;

    int fd = accept(server_fd, NULL, NULL);
    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return;
    }
    if (fd >= MAX_CLIENTS_FD) {
        fprintf(stderr, "fd %d >= MAX_CLIENTS_FD, rejecting\n", fd);
        close(fd);
        return;
    }
    if (set_non_blocking(fd) == -1) {
        close(fd);
        return;
    }

    state->is_client[fd] = true;
    printf("client connected (fd=%d)\n", fd);

    if (el_add(el, fd, on_read) == -1) {
        close(fd);
        state->is_client[fd] = false;
    }
}

int run_kqueue_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[kqueue] listening on port %d\n", PORT);

    kq_state_t state = {0};
    event_loop_t el;

    if (el_init(&el, &state) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    if (el_add(&el, server_fd, on_accept) == -1) {
        close(server_fd);
        el_cleanup(&el);
        return EXIT_FAILURE;
    }

    el_run(&el);

    for (int fd = 0; fd < MAX_CLIENTS_FD; fd++) {
        if (state.is_client[fd])
            close(fd);
    }
    close(server_fd);
    el_cleanup(&el);
    return EXIT_FAILURE;
}
