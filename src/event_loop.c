#include "event_loop.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_EVENTS 64

int el_init(event_loop_t *el, void *ctx)
{
    memset(el->handlers, 0, sizeof(el->handlers));
    el->ctx = ctx;

    el->kq = kqueue();
    if (el->kq == -1)
        return -1;

    return 0;
}

int el_add(event_loop_t *el, int fd, el_handler_fn handler)
{
    if (fd < 0 || fd >= MAX_FDS)
        return -1;

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(el->kq, &ev, 1, NULL, 0, NULL) == -1)
        return -1;

    el->handlers[fd] = handler;
    return 0;
}

void el_remove(event_loop_t *el, int fd)
{
    if (fd < 0 || fd >= MAX_FDS)
        return;

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(el->kq, &ev, 1, NULL, 0, NULL);
    el->handlers[fd] = NULL;
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
            if (el->handlers[fd])
                el->handlers[fd](el, fd);
        }
    }
}

void el_cleanup(event_loop_t *el)
{
    close(el->kq);
}
