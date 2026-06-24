# common

Socket setup shared by every phase. Three functions and a handful of
constants, nothing more.

| Function | What it does |
|----------|--------------|
| `create_listener()` | socket + `SO_REUSEADDR` + bind to port 9999 + listen |
| `set_non_blocking(fd)` | flip `O_NONBLOCK` on an fd |
| `write_all(fd, buf, len)` | write the whole buffer, handling short writes and `EINTR` |

## Why this is shared but the event loop is not

This boilerplate is byte-for-byte identical in a blocking server and a
kqueue server. It is not what the phases are about, so sharing it keeps
each `server.c` focused on the one thing that phase teaches.

The event loop is the opposite. How a server waits for I/O is the whole
point of this repo, and it changes completely from one phase to the next.
So phases 05 to 07 each spell out their own loop inline rather than hiding
it behind a shared abstraction. You read each `server.c` top to bottom and
see the real `kevent` / `epoll_wait` calls in context.
