#include "common.h"

ssize_t write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += n;
    }
    return (ssize_t)off;
}

size_t format_response(char *out, const char *data, size_t len)
{
    memcpy(out, PREFIX, PREFIX_LEN);
    memcpy(out + PREFIX_LEN, data, len);
    memcpy(out + PREFIX_LEN + len, SUFFIX, SUFFIX_LEN);
    return PREFIX_LEN + len + SUFFIX_LEN;
}

int set_non_blocking(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

int create_listener(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        goto fail;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind");
        goto fail;
    }

    if (listen(fd, BACKLOG) == -1) {
        perror("listen");
        goto fail;
    }

    return fd;

fail:
    close(fd);
    return -1;
}
