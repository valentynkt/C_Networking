# C_Networking

Five phases of a TCP echo server in C, from one-client-at-a-time blocking I/O up
to a kqueue event loop with a length-prefixed wire protocol and backpressure.
Each mode is a separate, runnable server — built to understand how a non-blocking
server actually works, one I/O model at a time. No dependencies beyond libc.

## The five phases

| Mode | What it is | What it teaches |
|---|---|---|
| `blocking` | one client at a time | why a blocking `accept`/`read` can't scale |
| `nonblocking` | busy-poll every fd | non-blocking sockets — and why polling burns 100% CPU |
| `select` | kernel-multiplexed | readiness notification; the `FD_SETSIZE` / O(n)-scan ceiling |
| `kqueue` | event loop | O(ready) dispatch, register-once, no fd-count limit |
| `framing` | length-prefixed protocol | message framing + write buffers + backpressure |

```sh
make                  # builds ./netpractice  (-Wall -Wextra -Werror, ASan+UBSan)
./netpractice kqueue  # mode: blocking | nonblocking | select | kqueue | framing
# in another shell:
nc localhost 9999
```

## The progression, and why each step exists

**blocking** — `accept`, then `read`/`write` in a loop. One slow client blocks
everyone. The baseline that motivates everything after it.

**nonblocking** — `O_NONBLOCK` makes the syscalls return `EAGAIN` instead of
sleeping, so one thread can juggle many fds — but spinning over all of them burns
a core for nothing. Motivates readiness notification.

**select** — let the kernel say which fds are ready. One thread, many clients, no
busy-poll. The wall: `select` rebuilds and re-scans the whole fd set every call
(O(n)), and `FD_SETSIZE` caps you.

**kqueue** — register each fd once; the kernel returns only the ready ones.
O(ready), not O(total), no fd-count ceiling. This phase introduces a small
reusable `event_loop` abstraction (read/write handler tables + dispatch) that the
framing phase builds on.

**framing** — TCP is a byte stream, not messages. Every frame is a 4-byte
big-endian length prefix followed by the payload. The read side accumulates bytes
and extracts complete frames; the write side queues responses per-client,
registers `EVFILT_WRITE` only when there's data to send, and applies
**backpressure** — if a client's write buffer fills, frame extraction stops until
it drains. The non-trivial part of any real server.

## The event loop

`event_loop.c` wraps kqueue behind `el_add` / `el_add_write` / `el_remove` /
`el_run`, with per-fd read and write handler tables. The kqueue and framing
servers are both written against it — the abstraction that turns "an example"
into "a server."

## Out of scope

A learning ladder, not a product. No TLS, no HTTP, no threads, no Linux `epoll`
backend (kqueue is macOS/BSD; the port is a small swap), no pipelining. The point
is the I/O models, end to end.

## References

- *The Linux Programming Interface*, Michael Kerrisk — sockets, non-blocking I/O, `select`.
- BSD man pages: `kqueue(2)`, `kevent(2)`, `fcntl(2)`.
- Beej's Guide to Network Programming.
