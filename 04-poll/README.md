# 04 - poll

select() without the bitmask.

## The idea

`poll()` does the same job as `select()`, it sleeps until an fd is ready,
but takes an array of `struct pollfd` instead of a fixed bitmask. Each
entry names an fd, the events you want (`POLLIN`), and the events that
actually fired (`revents`). Slot 0 holds the listener, the rest are
clients.

## Key calls

- `poll(fds, nfds, -1)` blocks until something in the array is ready.
- `fds[i].events` is what you watch for, `fds[i].revents` is what happened.
- Disconnects use swap-with-last, walking the array high to low so the
  swap never skips an entry.

## What got better than select()

- **No FD_SETSIZE cap.** The array is sized by you, so a high-numbered fd
  is no problem.
- **No per-loop rebuild.** `poll()` reports readiness in `revents` and
  leaves your fd list untouched, so you reuse the same array each time.

## Build and run

```sh
make
./server
```

## Feel the problem

`poll()` is nicer than `select()`, but it has not changed the fundamental
cost. Every call still hands the kernel the entire fd list, the kernel
still checks each one, and you still scan the whole array for ready
entries. With 10,000 mostly-idle connections you do 10,000 units of work
to service the handful that spoke. That is the O(n) wall.

## Next

[05-kqueue](../05-kqueue) breaks the O(n) wall. You register each fd once,
and the kernel returns only the fds that are actually ready, so the work
per wakeup is O(ready) instead of O(total).
