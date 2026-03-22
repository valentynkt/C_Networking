#include "blocking.h"
#include "common.h"

/*
 * Phase 1: Blocking TCP Echo Server
 *
 * One client at a time. accept() blocks until a connection arrives,
 * read() blocks until data arrives. Second client waits in the
 * kernel's backlog queue until the first disconnects.
 *
 * Feel the problem: connect two telnet sessions simultaneously.
 * The second one hangs.
 */

static void handle_client(int fd)
{
    char buf[BUF_SIZE];

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n == 0) {
            printf("client disconnected (fd=%d)\n", fd);
            return;
        }
        if (n == -1) {
            if (errno == EINTR)
                continue;
            perror("read");
            return;
        }

        char response[BUF_SIZE + PREFIX_LEN + SUFFIX_LEN];
        size_t resp_len = format_response(response, buf, (size_t)n);
        if (write_all(fd, response, resp_len) == -1) {
            perror("write");
            return;
        }
    }
}

int run_blocking_server(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return EXIT_FAILURE;

    printf("[blocking] listening on port %d\n", PORT);

    for (;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        printf("client connected (fd=%d)\n", client_fd);
        handle_client(client_fd);
        close(client_fd);
    }

    close(server_fd);
    return EXIT_SUCCESS;
}
