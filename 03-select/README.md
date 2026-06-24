# 03 - select

Let the kernel do the waiting.

## The idea

Keep a set of fds we care about and call `select()`. It sleeps until at
least one of them is readable, then tells us which. An idle server now
parks in the kernel at roughly 0% CPU. This is the direct fix for phase
02's busy loop.

## Key calls

- `select(maxfd+1, &readfds, ...)` blocks until an fd is ready.
- `FD_SET` / `FD_ISSET` / `FD_CLR` manage the bitmask.
- The set is copied every loop because `select()` overwrites it.

## Build and run

```sh
make
./server
```

Idle CPU is now near zero, unlike phase 02. Connect a few clients with
`nc localhost 9999`, they are all served.

## Feel the problem

`select()` is showing its age:

- **FD_SETSIZE cap.** The fd set is a fixed bitmask, 1024 slots on most
  systems. Connection number 1024 or higher cannot be watched at all, so
  this server rejects it.
- **O(n) every wakeup.** Even if one client out of a thousand has data,
  you rebuild the whole set and then scan every fd to find the ready one.
  The cost grows with total connections, not with how many are active.

## Next

[04-poll](../04-poll) keeps the kernel doing the waiting but swaps the
bitmask for an array, which removes the FD_SETSIZE cap and the per-loop
rebuild. The O(n) scan stays, and kqueue/epoll remove that later.
