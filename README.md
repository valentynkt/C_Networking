# building-tcp-servers-in-c

*One TCP server, built seven times. Blocking sockets up to an event loop with its own wire protocol.*

Seven phases of a TCP echo server in C, from one-client-at-a-time blocking
I/O up to a kqueue and epoll event loop with a length-prefixed wire protocol
and backpressure. Each phase is a separate, runnable server in its own
folder, built to understand how a non-blocking server actually works, one
I/O model at a time. No dependencies beyond libc.

It is the [C10K problem](http://www.kegel.com/c10k.html) told as a sequence
of small, readable programs: every phase hits a concrete wall, and the next
phase is the answer to it.

**What's interesting here:**

- [Seven I/O models, one folder each](#the-seven-phases): `blocking →
  nonblocking → select → poll → kqueue → epoll → framing`, every one
  runnable on its own.
- [Why each step exists](#how-each-phase-works): every phase is motivated by
  a concrete wall the previous one hit.
- [The event loop, written out in the open](#the-shared-piece): phases 05 to
  07 spell their loop out inline instead of hiding it, because how a server
  waits for I/O is the whole subject.
- [Framing and backpressure](#how-each-phase-works): length-prefixed
  messages over a byte stream, with per-client write buffers that stop
  reading when full.

## The seven phases

| # | Phase | What it is | What it teaches |
|---|---|---|---|
| 01 | [blocking](01-blocking) | one client at a time | why a blocking `accept`/`read` cannot scale |
| 02 | [nonblocking](02-nonblocking) | busy-poll every fd | non-blocking sockets, and why polling burns 100% CPU |
| 03 | [select](03-select) | kernel-multiplexed | readiness notification, and the `FD_SETSIZE` / O(n) ceiling |
| 04 | [poll](04-poll) | `pollfd` array | the same idea without the bitmask cap, still O(n) |
| 05 | [kqueue](05-kqueue) | event loop, macOS/BSD | O(ready) dispatch, register once, no fd-count limit |
| 06 | [epoll](06-epoll) | event loop, Linux | the same design on the other big kernel |
| 07 | [framing](07-framing) | length-prefixed protocol | message framing, write buffers, backpressure |

Phases 05 and 06 are the same idea on two kernels, not two steps. Read
whichever matches your machine; the other is its mirror.

## Demo

Each phase lives in its own folder and builds to a local `./server` on port
9999:

```sh
cd 05-kqueue
make                  # builds ./server  (-Wall -Wextra -Werror, ASan + UBSan)
./server
```

The echo phases speak raw bytes, so talk to them with `nc`:

```sh
$ nc localhost 9999
hello
hello                 # echoed back
```

The `framing` server speaks a length-prefixed protocol instead of raw echo,
so drive it with the bundled client:

```sh
cd 07-framing
make && ./server &
python3 test_framing.py
```

## How each phase works

**blocking.** `accept`, then `read`/`write` in a loop. One slow client
blocks everyone. The baseline that motivates everything after it.

**nonblocking.** `O_NONBLOCK` makes the syscalls return `EAGAIN` instead of
sleeping, so one thread can juggle many fds. But spinning over all of them
burns a core for nothing, which motivates readiness notification.

**select.** Let the kernel say which fds are ready. One thread, many
clients, no busy-poll. The wall: `select` rebuilds and re-scans the whole fd
set every call (O(n)), and `FD_SETSIZE` caps you at around 1024 fds.

**poll.** The same readiness idea with an array of `struct pollfd` instead
of a fixed bitmask. That removes the `FD_SETSIZE` cap and the
rebuild-every-call dance. It is still O(n) though: the kernel scans every fd
you pass, and so do you.

**kqueue.** Register each fd once; the kernel returns only the ready ones.
O(ready), not O(total), with no fd-count ceiling. This is where the event
loop becomes genuinely different from the earlier phases, so it is written
out inline rather than hidden behind a helper. macOS and BSD.

**epoll.** The Linux counterpart of kqueue, identical in spirit:
`epoll_create1`, `epoll_ctl`, `epoll_wait`. The kqueue README maps the two
APIs call for call.

**framing.** TCP is a byte stream, not messages. Every frame is a 4-byte
big-endian length prefix followed by the payload. The read side accumulates
bytes and extracts complete frames; the write side queues responses per
client, registers `EVFILT_WRITE` only when there is data to send, and
applies **backpressure**: if a client's write buffer fills, frame extraction
stops until it drains. The non-trivial part of any real server.

## The shared piece

Only the socket setup is shared, in [`common/`](common): create a listening
socket, set non-blocking, write a full buffer. That code is byte-for-byte
identical in every phase and is not what any phase is teaching, so it lives
in one place.

The event loop is the opposite. How a server waits for I/O is the entire
subject of this repo, and it changes completely from one phase to the next.
So phases 05 to 07 each write their loop out inline, with the real `kevent`
and `epoll_wait` calls visible in context, instead of sharing one
abstraction that would hide the very thing you came to read. An earlier
version of this repo shared an `event_loop.c`; pulling it back into each
phase is what lets them stand alone.

## Folder map

| Folder | Phase |
|---|---|
| [`common/`](common) | shared socket setup: listen socket, bind, set-non-blocking, write-all |
| [`01-blocking/`](01-blocking) | accept-and-serve, one client at a time |
| [`02-nonblocking/`](02-nonblocking) | non-blocking fds, busy-poll over all of them |
| [`03-select/`](03-select) | `select`-multiplexed readiness, single thread |
| [`04-poll/`](04-poll) | `poll`-multiplexed readiness with a `pollfd` array |
| [`05-kqueue/`](05-kqueue) | kqueue event loop, register-once dispatch (macOS/BSD) |
| [`06-epoll/`](06-epoll) | epoll event loop, the Linux twin (Linux) |
| [`07-framing/`](07-framing) | length-prefixed protocol, write buffers, backpressure |

## Build

Every phase has the same one-line build:

```sh
cd <phase>
make            # ./server
make clean      # remove the binary
```

C17, POSIX, no dependencies beyond libc. Built with `-Wall -Wextra -Werror`
and AddressSanitizer plus UndefinedBehaviorSanitizer on by default, so
memory and undefined-behavior bugs show up during normal testing.

Platforms: phases 01 to 04 are portable POSIX; 05 and 07 are macOS and BSD
(kqueue); 06 is Linux (epoll). The epoll README shows how to build it in a
container on a Mac.

## Out of scope

A learning ladder, not a product. No TLS, no HTTP, no threads, no
`io_uring`, no pipelining beyond what framing already handles. The point is
the I/O models, end to end.

## References

- *The Linux Programming Interface*, Michael Kerrisk: sockets, non-blocking I/O, `select`, `poll`, `epoll`.
- *Beej's Guide to Network Programming*.
- BSD man pages: `kqueue(2)`, `kevent(2)`, `fcntl(2)`. Linux man pages: `epoll(7)`, `poll(2)`.
- Dan Kegel, [The C10K problem](http://www.kegel.com/c10k.html).
