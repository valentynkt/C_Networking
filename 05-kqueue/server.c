#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * kqueue echo server (macOS / BSD).
 *
 * Register each fd with the kernel once, then call kevent() to get back
 * only the fds that are actually ready. No FD_SETSIZE cap, and the work
 * per wakeup is O(ready) rather than O(total). This is the jump that
 * select() and poll() were building toward: with 10,000 idle connections
 * and one active, kevent() hands you exactly that one.
 *
 * The event loop is written out here rather than hidden behind a shared
 * helper, so you can see the raw kevent() calls in context. Linux has the
 * same model under a different name, see 06-epoll.
 */

#define MAX_EVENTS 64

static void handle_client(int kq, int fd)
{
    char buf[BUF_SIZE];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n > 0) {
        if (write_all(fd, buf, (size_t)n) != -1)
            return;
        perror("write");
    } else if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return;
        perror("read");
    }

    /* n == 0 (clean close) or an error: drop the client. Deleting the fd
       from the kqueue is automatic on close(), but we are explicit. */
    printf("client disconnected (fd=%d)\n", fd);
    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);
    close(fd);
}

static void accept_client(int kq, int server_fd)
{
    int fd = accept(server_fd, NULL, NULL);
    if (fd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            perror("accept");
        return;
    }
    if (set_non_blocking(fd) == -1) {
        close(fd);
        return;
    }

    struct kevent ev;
    EV_SET(&ev, fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
        perror("kevent EV_ADD");
        close(fd);
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

    int kq = kqueue();
    if (kq == -1) {
        perror("kqueue");
        close(server_fd);
        return 1;
    }

    /* Watch the listener for readability; that means a pending connection. */
    struct kevent ev;
    EV_SET(&ev, server_fd, EVFILT_READ, EV_ADD, 0, 0, NULL);
    if (kevent(kq, &ev, 1, NULL, 0, NULL) == -1) {
        perror("kevent EV_ADD listener");
        close(server_fd);
        close(kq);
        return 1;
    }

    printf("[kqueue] listening on port %d\n", PORT);

    struct kevent events[MAX_EVENTS];
    for (;;) {
        int n = kevent(kq, NULL, 0, events, MAX_EVENTS, NULL);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("kevent");
            break;
        }
        for (int i = 0; i < n; i++) {
            int fd = (int)events[i].ident;
            if (fd == server_fd)
                accept_client(kq, server_fd);
            else
                handle_client(kq, fd);
        }
    }

    close(server_fd);
    close(kq);
    return 1;
}
