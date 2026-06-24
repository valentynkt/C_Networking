#include "common.h"
#include "frame.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Length-prefixed framing over a kqueue event loop.
 *
 * This is 05-kqueue with a real wire protocol bolted on. A plain echo
 * server gets away with ignoring two hard problems. This one does not:
 *
 *   1. Framing. TCP is a byte stream, not messages. A read() can return a
 *      partial message or several at once. We accumulate bytes in a
 *      per-client read buffer and pull out complete frames (see frame.h
 *      for the wire format).
 *
 *   2. Backpressure. Writing naively to a slow client blocks the whole
 *      server. Instead each client has a write buffer. We queue the
 *      response, ask kqueue to tell us when the socket is writable
 *      (EVFILT_WRITE), and drain it then. If the write buffer fills, we
 *      stop pulling frames off the read side until it drains again, so a
 *      slow reader cannot make us buffer without bound.
 *
 * The kqueue loop is inlined here, same as phase 05, so the whole flow
 * reads top to bottom.
 */

#define MAX_EVENTS 64

typedef struct {
    int      kq;
    client_t clients[MAX_CLIENTS_FD];
} server_t;

/* EV_ADD/EV_DELETE the write filter for a client. EV_ADD is idempotent. */
static void watch_write(server_t *s, int fd, bool on)
{
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_WRITE, on ? EV_ADD : EV_DELETE, 0, 0, NULL);
    kevent(s->kq, &ev, 1, NULL, 0, NULL);
}

static void remove_client(server_t *s, int fd)
{
    printf("client disconnected (fd=%d)\n", fd);
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(s->kq, &ev, 1, NULL, 0, NULL);
    EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
    kevent(s->kq, &ev, 1, NULL, 0, NULL);
    close(fd);
    s->clients[fd] = (client_t){0};
}

/* Append bytes to the write buffer, compacting sent bytes out of the way
   if needed. Returns false when the buffer is full. */
static bool queue_write(server_t *s, int fd, const char *data, size_t len)
{
    client_t *c = &s->clients[fd];

    if (c->wlen + len > WBUF_SIZE && c->woff > 0) {
        size_t pending = c->wlen - c->woff;
        memmove(c->wbuf, c->wbuf + c->woff, pending);
        c->wlen = pending;
        c->woff = 0;
    }
    if (c->wlen + len > WBUF_SIZE)
        return false;

    memcpy(c->wbuf + c->wlen, data, len);
    c->wlen += len;
    watch_write(s, fd, true);
    return true;
}

/* Pull every complete frame out of the read buffer and queue it back as
   the echo. Stops early on an incomplete frame or a full write buffer. */
static void process_buffer(server_t *s, int fd)
{
    client_t *c = &s->clients[fd];

    while (c->len >= FRAME_HDR_SIZE) {
        uint32_t net_len;
        memcpy(&net_len, c->buf, FRAME_HDR_SIZE);
        uint32_t payload_len = ntohl(net_len);

        if (payload_len > MSG_MAX) {
            remove_client(s, fd);   /* protocol violation, drop the client */
            return;
        }
        if (c->len < FRAME_HDR_SIZE + payload_len)
            return;                 /* frame not fully arrived yet */

        size_t frame_size = FRAME_HDR_SIZE + payload_len;
        if (!queue_write(s, fd, c->buf, frame_size))
            return;                 /* write buffer full, resume after drain */

        memmove(c->buf, c->buf + frame_size, c->len - frame_size);
        c->len -= frame_size;
    }
}

/* kqueue says the socket is writable: drain the write buffer. */
static void on_writable(server_t *s, int fd)
{
    client_t *c = &s->clients[fd];

    ssize_t n = write(fd, c->wbuf + c->woff, c->wlen - c->woff);
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        perror("write");
        remove_client(s, fd);
        return;
    }
    c->woff += (size_t)n;

    if (c->woff == c->wlen) {
        c->wlen = 0;
        c->woff = 0;
        watch_write(s, fd, false);
        process_buffer(s, fd);  /* may have stalled earlier on a full buffer */
    }
}

/* kqueue says the socket is readable: pull in bytes, extract frames. */
static void on_readable(server_t *s, int fd)
{
    client_t *c = &s->clients[fd];
    if (!c->active)
        return;

    ssize_t n = read(fd, c->buf + c->len, sizeof(c->buf) - c->len);
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
    c->len += (size_t)n;
    process_buffer(s, fd);
}

static void accept_client(server_t *s, int server_fd)
{
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

    s->clients[fd] = (client_t){ .active = true };
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(s->kq, &ev, 1, NULL, 0, NULL) == -1) {
        perror("kevent EV_ADD");
        close(fd);
        s->clients[fd] = (client_t){0};
        return;
    }
    printf("client connected (fd=%d)\n", fd);
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

    /* server_t holds a read and write buffer per fd, so it is large
       (a few MB). Keep it in BSS instead of on the stack. */
    static server_t s;
    memset(&s, 0, sizeof(s));

    s.kq = kqueue();
    if (s.kq == -1) {
        perror("kqueue");
        close(server_fd);
        return 1;
    }

    struct kevent ev;
    EV_SET(&ev, server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(s.kq, &ev, 1, NULL, 0, NULL) == -1) {
        perror("kevent EV_ADD listener");
        close(server_fd);
        close(s.kq);
        return 1;
    }

    printf("[framing] listening on port %d\n", PORT);

    struct kevent events[MAX_EVENTS];
    for (;;) {
        int n = kevent(s.kq, NULL, 0, events, MAX_EVENTS, NULL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("kevent");
            break;
        }
        for (int i = 0; i < n; i++) {
            int fd = (int)events[i].ident;
            if (fd == server_fd)
                accept_client(&s, server_fd);
            else if (events[i].filter == EVFILT_READ)
                on_readable(&s, fd);
            else if (events[i].filter == EVFILT_WRITE)
                on_writable(&s, fd);
        }
    }

    close(server_fd);
    close(s.kq);
    return 1;
}
