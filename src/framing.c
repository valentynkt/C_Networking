#include "framing.h"
#include "common.h"
#include "event_loop.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>

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

static void on_read(event_loop_t *el, int fd);

/* TODO: implement remove_client — el_remove, close, reset client state */

/* TODO: implement send_framed — write [4-byte length][payload] to fd */

/* TODO: implement process_buffer — the framing loop:
 *   while buf has >= FRAME_HDR_SIZE bytes:
 *     extract payload_len (ntohl)
 *     validate payload_len <= MSG_MAX
 *     if buf has full frame: send_framed echo, memmove compact, continue
 *     else: break (wait for more data)
 */

/* TODO: implement on_accept — accept, set_non_blocking, el_add(el, fd, on_read), init client */
static void on_accept(event_loop_t *el, int server_fd)
{
    (void)el; (void)server_fd; (void)on_read;
}

/* TODO: implement on_read — read into client->buf + client->len, then process_buffer */
static void on_read(event_loop_t *el, int fd)
{
    (void)el; (void)fd;
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
