#include "framing.h"
#include "common.h"
#include "event_loop.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CLIENTS_FD 1024
#define MSG_MAX 4096
#define FRAME_HDR_SIZE 4

/*
 * Phase 5: Wire Protocol — Length-Prefixed Framing (kqueue)
 *
 * Same event loop as Phase 4. What changes: how we interpret bytes.
 *
 * Wire format:
 *   [4 bytes: payload length, big-endian uint32][payload bytes]
 *
 * TCP is a byte stream — one read() may return half a message,
 * two messages, or 1.5 messages. Accumulate per client, parse frames.
 */

typedef struct {
    bool active;
    char buf[MSG_MAX + FRAME_HDR_SIZE];
    size_t len;
} client_t;

typedef struct {
    client_t clients[MAX_CLIENTS_FD];
} framing_state_t;

static void remove_client(event_loop_t *el, int fd)
{
    framing_state_t *state = el->ctx;
    printf("client disconnected (fd=%d)\n", fd);
    el_remove(el, fd);
    close(fd);
    state->clients[fd].active = false;
}

static void send_framed(event_loop_t *el, int fd, const char *payload, uint32_t len)
{
    uint32_t net_len = htonl(len);
    char frame[FRAME_HDR_SIZE + MSG_MAX];
    memcpy(frame, &net_len, FRAME_HDR_SIZE);
    memcpy(frame + FRAME_HDR_SIZE, payload, len);
    if (write_all(fd, frame, FRAME_HDR_SIZE + len) == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("write");
            remove_client(el, fd);
        }
    }
}
static void process_buffer(event_loop_t *el, int fd)
{
    framing_state_t *state = el->ctx;
    client_t *c = &state->clients[fd];
    while (true) {
        if (c->len < 4) {
            return;
        }
        uint32_t net_len;
        memcpy(&net_len, c->buf, FRAME_HDR_SIZE);
        uint32_t payload_len = ntohl(net_len);
        if (payload_len > MSG_MAX) {
            remove_client(el, fd);
            return;
        } else if (c->len < 4 + payload_len) {
            printf("not enough for full frame: (len = %d)\n", (int)c->len);
            return;
        }
        send_framed(el, fd, c->buf + FRAME_HDR_SIZE, payload_len);
        size_t frame_size = FRAME_HDR_SIZE + payload_len;

        memmove(c->buf, c->buf + frame_size, c->len - frame_size);
        c->len -= frame_size;
    }
}
static void on_read(event_loop_t *el, int fd)
{
    framing_state_t *state = el->ctx;
    client_t *c = &state->clients[fd];
    if (!c->active) {
        printf("client is inactive (fd=%d)\n", fd);
        return;
    }
    ssize_t n = read(fd, c->buf + c->len, sizeof(c->buf) - c->len);

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

    c->len += n;
    process_buffer(el, fd);
}

static void on_accept(event_loop_t *el, int server_fd)
{
    framing_state_t *state = el->ctx;

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

    state->clients[fd] = (client_t){.active = true};

    printf("client connected (fd=%d)\n", fd);

    if (el_add(el, fd, on_read) == -1) {
        close(fd);
        state->clients[fd].active = false;
    }
}

int run_framing_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[framing] listening on port %d\n", PORT);

    framing_state_t state = {0};
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
        if (state.clients[fd].active)
            close(fd);
    }
    close(server_fd);
    el_cleanup(&el);
    return EXIT_FAILURE;
}
