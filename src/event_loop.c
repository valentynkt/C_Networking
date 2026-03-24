#include "event_loop.h"
#include <errno.h>
#include <stdio.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EVENTS 64

int el_init(event_loop_t *el, int server_fd,
            el_handler_fn on_accept, el_handler_fn on_read,
            void *ctx)
{
    el->server_fd = server_fd;
    el->on_accept = on_accept;
    el->on_read = on_read;
    el->ctx = ctx;

    el->kq = kqueue();
    if (el->kq == -1)
        return -1;

    return el_register_read(el, server_fd);
}

int el_register_read(event_loop_t *el, int fd)
{
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    return kevent(el->kq, &ev, 1, NULL, 0, NULL) == -1 ? -1 : 0;
}

void el_remove_fd(event_loop_t *el, int fd)
{
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(el->kq, &ev, 1, NULL, 0, NULL);
}

int el_run(event_loop_t *el)
{
    struct kevent events[MAX_EVENTS];

    for (;;) {
        int n = kevent(el->kq, NULL, 0, events, MAX_EVENTS, NULL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("kevent");
            return -1;
        }

        for (int i = 0; i < n; i++) {
            int fd = (int)events[i].ident;
            if (fd == el->server_fd)
                el->on_accept(el, fd);
            else
                el->on_read(el, fd);
        }
    }
}

void el_cleanup(event_loop_t *el)
{
    close(el->kq);
    close(el->server_fd);
}
