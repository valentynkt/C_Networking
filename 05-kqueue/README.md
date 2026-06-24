# 05 - kqueue

Register once, get back only what is ready. macOS and BSD.

## The idea

`select()` and `poll()` make you describe the entire fd set on every call.
`kqueue` flips that around. You register each fd once with `EV_SET` +
`kevent()`, and from then on `kevent()` returns only the fds that are
ready right now. With 10,000 connections where one has data, you get back
one event, not 10,000.

This is the first phase whose event loop is genuinely different, so it is
written out inline here. There is no shared loop helper to peek behind.

## Key calls

- `kqueue()` creates the kernel event queue.
- `EV_SET(&ev, fd, EVFILT_READ, EV_ADD, ...)` registers interest in an fd.
- `kevent(kq, NULL, 0, events, ...)` blocks and returns the ready events.
- `EV_DELETE` removes an fd (also automatic when you `close()` it).

## What got better than poll()

- **O(ready), not O(total).** The kernel keeps the watch list, so a wakeup
  costs work proportional to how many fds are active, not how many exist.
- **No per-call setup.** You register an fd once and forget about it.

## Build and run

```sh
make
./server
```

```sh
nc localhost 9999
```

## Platform note

`kqueue` is a BSD interface, available on macOS and the BSDs. On Linux the
exact same model exists under the name epoll. The next phase is the Linux
twin of this one.

## Next

[06-epoll](../06-epoll) is this server rewritten with Linux's `epoll`.
Then [07-framing](../07-framing) builds a real wire protocol on top of the
kqueue loop you just read.
