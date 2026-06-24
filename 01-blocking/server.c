#include "common.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Blocking echo server: one client at a time.
 *
 * accept() blocks until a connection arrives, and read() blocks until the
 * client sends something. While we are busy talking to one client, a
 * second connection just waits in the kernel's backlog queue. It does not
 * get served until the first client disconnects.
 *
 * This is the simplest server you can write, and the reason every other
 * phase exists. Open two clients at once and watch the second one hang.
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
        if (write_all(fd, buf, (size_t)n) == -1) {
            perror("write");
            return;
        }
    }
}

int main(void)
{
    int server_fd = create_listener();
    if (server_fd == -1)
        return 1;

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
}
