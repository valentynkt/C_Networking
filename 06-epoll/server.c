#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * epoll echo server (Linux).
 *
 * The Linux twin of 05-kqueue. Same model exactly: register each fd once,
 * then ask the kernel for the ready ones, O(ready) per wakeup. Only the
 * spelling of the syscalls changes. Read this side by side with the kqueue
 * version and the mapping is one to one:
 *
 *   kqueue()                     ->  epoll_create1()
 *   EV_SET(... EVFILT_READ ...)  ->  struct epoll_event { .events=EPOLLIN }
 *   kevent(... EV_ADD ...)       ->  epoll_ctl(EPOLL_CTL_ADD)
 *   kevent(... EV_DELETE ...)    ->  epoll_ctl(EPOLL_CTL_DEL)
 *   kevent(... &events ...)      ->  epoll_wait()
 *
 * Linux only. This does not compile on macOS (there is no <sys/epoll.h>).
 * Build it on a Linux machine or in a container.
 */

#define MAX_EVENTS 64

static void handle_client(int epfd, int fd)
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

    /* n == 0 (clean close) or an error: drop the client. */
    printf("client disconnected (fd=%d)\n", fd);
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

static void accept_client(int epfd, int server_fd)
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

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = fd };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl ADD");
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

    int epfd = epoll_create1(0);
    if (epfd == -1) {
        perror("epoll_create1");
        close(server_fd);
        return 1;
    }

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = server_fd };
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl ADD listener");
        close(server_fd);
        close(epfd);
        return 1;
    }

    printf("[epoll] listening on port %d\n", PORT);

    struct epoll_event events[MAX_EVENTS];
    for (;;) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            if (fd == server_fd)
                accept_client(epfd, server_fd);
            else
                handle_client(epfd, fd);
        }
    }

    close(server_fd);
    close(epfd);
    return 1;
}
