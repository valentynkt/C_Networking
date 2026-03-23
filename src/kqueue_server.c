#include "kqueue_server.h"
#include "common.h"
#include <stdbool.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#define MAX_CLIENTS_FD 1024

/*
 * Phase 4: kqueue Event Loop Echo Server
 *
 * Register fds once. Kernel returns only ready fds.
 * O(ready) not O(total). No FD_SETSIZE limit.
 *
 * TODO: implement
 */

typedef struct {
    int kq;
    int server_fd;
    bool is_client[MAX_CLIENTS_FD]; // is_connected[fd] = true means active client
} server_t;

static int init_server(server_t *s, int server_fd)
{
    s->server_fd = server_fd;
    memset(s->is_client, 0, sizeof(s->is_client));
    struct kevent ev;
    EV_SET(&ev, server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    s->kq = kqueue();
    if (s->kq == -1) {
        return EXIT_FAILURE;
    }
    if (kevent(s->kq, &ev, 1, NULL, 0, NULL) == -1) {
        close(s->kq);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static void shutdown_server(server_t *s)
{
    for (int fd = 0; fd < MAX_CLIENTS_FD; fd++) {
        if (s->is_client[fd])
            close(fd);
    }
    close(s->kq);
    close(s->server_fd);
}
static void remove_client(server_t *s, int fd)
{
    printf("client disconnected (fd=%d)\n", fd);
    close(fd);
    s->is_client[fd] = false;
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

static int accept_client(server_t *s)
{
    int fd = accept(s->server_fd, NULL, NULL);
    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return EXIT_FAILURE;
    }
    if (fd >= MAX_CLIENTS_FD) {
        fprintf(stderr, "fd %d >= MAX_CLIENTS_FD, rejecting\n", fd);
        close(fd);
        return EXIT_FAILURE;
    }
    if (set_non_blocking(fd) == -1) {
        close(fd);
        return EXIT_FAILURE;
    }
    s->is_client[fd] = true;
    printf("client connected (fd=%d)\n", fd);
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(s->kq, &ev, 1, NULL, 0, NULL) == -1) {
        close(fd);
        s->is_client[fd] = false;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
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

    server_t s;
    if (init_server(&s, server_fd) == EXIT_FAILURE) {
        shutdown_server(&s);
        return EXIT_FAILURE;
    }

    for (;;) {
        struct kevent read_events[64];
        int events_n = kevent(s.kq, NULL, 0, read_events, 64, NULL);
        if (events_n == -1) {
            if (errno == EINTR)
                continue;
            perror("kevent");
            break;
        }
        for (int i = 0; i < events_n; i++) {
            // event fro server has new clients.
            if ((int)read_events[i].ident == s.server_fd) {
                accept_client(&s);
            }
            // event for clients has read.
            else {
                int cur_client_fd = (int)read_events[i].ident;
                handle_client(&s, cur_client_fd);
            }
        }
    }
    shutdown_server(&s);
    return EXIT_FAILURE;
}
