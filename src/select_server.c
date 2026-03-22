#include "select_server.h"
#include "common.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Phase 3: select() Multiplexed Echo Server
 *
 * TODO: You will build this.
 *
 * The kernel does the waiting — select() blocks until at least one fd
 * is ready. CPU drops to ~0% when idle. This fixes Phase 2's busy-loop.
 *
 * Syscalls: select(), FD_ZERO(), FD_SET(), FD_ISSET(), FD_CLR()
 *
 * Key difference from Phase 2:
 *   Phase 2: you poll every fd yourself, millions of EAGAIN per second
 *   Phase 3: kernel tells you which fds have data, you only check those
 */

int run_select_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    if (set_non_blocking(server_fd) == -1) {
        close(server_fd);
        return EXIT_FAILURE;
    }

    printf("[select] listening on port %d\n", PORT);

    int clients[MAX_CLIENTS];
    int count = 0;
    int dead_fd;

    for (;;) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;
        for (int i = 0; i < count; i++) {
            FD_SET(clients[i], &readfds);
            if (clients[i] > max_fd) {
                max_fd = clients[i];
            }
        }
        int ready = select(max_fd + 1, &readfds, NULL, NULL, NULL);
        if (ready > 0) {
            if (FD_ISSET(server_fd, &readfds)) {
                ready -= 1;
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
            }
            if (ready <= 0) {
                continue;
            }
            for (int i = 0; i < count; i++) {
                if (FD_ISSET(clients[i], &readfds)) {
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

                    if (write_all(clients[i], response, (size_t)(PREFIX_LEN + n + SUFFIX_LEN)) ==
                        -1) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                            continue;
                        perror("write");
                        goto remove_client;
                    }
                    continue;
                remove_client:
                    dead_fd = clients[i];
                    clients[i] = clients[count - 1];
                    count--;
                    i--;
                    close(dead_fd);
                }
            }

        } else if (ready == -1 && errno == EINTR) {
            continue;
        } else if (ready == -1) {
            goto server_fail;
        }
    }

    return EXIT_SUCCESS;

server_fail:
    perror("server_fail");
    close(server_fd);
    return EXIT_FAILURE;
}
