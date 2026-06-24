#include "common.h"

#include <errno.h>
#include <poll.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * poll() multiplexed echo server.
 *
 * Same deal as select(): the kernel sleeps until an fd is ready. The only
 * change is the data structure. Instead of a fixed bitmask we hand poll()
 * an array of struct pollfd, each holding an fd, the events we want
 * (POLLIN), and the events that fired (revents).
 *
 * Two things get better than select():
 *   - No FD_SETSIZE cap. The array is as big as we allocate, so high fd
 *     numbers are fine.
 *   - No rebuild each loop. poll() reports readiness in revents and leaves
 *     our fd list intact, so we reuse the same array.
 *
 * One thing stays the same: it is still O(n). poll() scans every fd we
 * pass, and we scan the whole array for ready ones, even when only one
 * client is active. Removing that scan is what kqueue and epoll are for.
 */

int main(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return 1;
    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return 1;
    }

    printf("[poll] listening on port %d\n", PORT);

    /* Slot 0 is always the listener; slots 1.. are clients. */
    struct pollfd fds[MAX_CLIENTS + 1];
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;
    nfds_t nfds = 1;

    for (;;) {
        int ready = poll(fds, nfds, -1);
        if (ready == -1) {
            if (errno == EINTR)
                continue;
            perror("poll");
            break;
        }

        if (fds[0].revents & POLLIN) {
            int client_fd = accept(server_fd, NULL, NULL);
            if (client_fd == -1) {
                if (errno != EAGAIN && errno != EWOULDBLOCK)
                    perror("accept");
            } else if (nfds > MAX_CLIENTS) {
                fprintf(stderr, "too many clients, rejecting fd %d\n", client_fd);
                close(client_fd);
            } else {
                fds[nfds].fd = client_fd;
                fds[nfds].events = POLLIN;
                fds[nfds].revents = 0;
                nfds++;
                printf("client connected (fd=%d)\n", client_fd);
            }
        }

        /* Walk clients high to low so swap-with-last removal is safe. */
        for (nfds_t i = nfds; i-- > 1;) {
            if (!(fds[i].revents & (POLLIN | POLLHUP | POLLERR)))
                continue;

            char buf[BUF_SIZE];
            ssize_t n = read(fds[i].fd, buf, sizeof(buf));
            if (n > 0) {
                if (write_all(fds[i].fd, buf, (size_t)n) != -1)
                    continue;
            } else if (n == -1 &&
                       (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
                continue;
            }

            /* n == 0 (closed), write failed, or a real read error: drop it. */
            printf("client disconnected (fd=%d)\n", fds[i].fd);
            close(fds[i].fd);
            fds[i] = fds[nfds - 1];
            nfds--;
        }
    }

    for (nfds_t i = 0; i < nfds; i++)
        close(fds[i].fd);
    return 1;
}
