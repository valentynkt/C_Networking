#include "nonblocking.h"
#include "common.h"
#include <string.h>

/*
 * Phase 2: Non-Blocking Busy-Poll Echo Server
 *
 * All fds set to O_NONBLOCK. Single flat loop: try accept, then
 * iterate all tracked clients. EAGAIN means "nothing ready, skip."
 *
 * Multiple clients work simultaneously. But CPU burns at 100%
 * even when idle — millions of EAGAIN returns per second.
 * This motivates select() and kqueue.
 *
 * Feel the problem: run `top` while the server is idle.
 */

int run_nonblocking_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[nonblocking] listening on port %d\n", PORT);

    int clients[MAX_CLIENTS];
    int count = 0;

    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("accept");
            }
        } else {
            if (set_non_blocking(client_fd) == -1) {
                close(client_fd);
            } else if (count < MAX_CLIENTS) {
                clients[count] = client_fd;
                count++;
            } else {
                close(client_fd);
            }
        }

        for (int i = 0; i < count; i++) {
            char buf[BUF_SIZE];
            char response[BUF_SIZE + PREFIX_LEN + SUFFIX_LEN];
            ssize_t n = read(clients[i], buf, sizeof(buf));
            if (n == 0) {
                printf("client disconnected (fd=%d)\n", clients[i]);
                goto remove_client;
            }
            if (n == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
                    continue;
                perror("read");
                goto remove_client;
            }

            memcpy(response, PREFIX, PREFIX_LEN);
            memcpy(response + PREFIX_LEN, buf, n);
            memcpy(response + PREFIX_LEN + n, SUFFIX, SUFFIX_LEN);

            if (write_all(clients[i], response, (size_t)(PREFIX_LEN + n + SUFFIX_LEN)) == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                perror("write");
                goto remove_client;
            }
            int dead_fd;
            continue;
        remove_client:
            dead_fd = clients[i];
            clients[i] = clients[count - 1];
            count--;
            i--;
            close(dead_fd);
        }
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
