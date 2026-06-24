# 06 - epoll

The Linux twin of kqueue.

## The idea

`epoll` is Linux's readiness API. It is the same model as `kqueue` from
phase 05: register each fd once, then ask the kernel for the ready ones,
O(ready) per wakeup. Nothing conceptual changes here. This phase exists so
the repo is honest about the fact that the two big platforms reach the same
design through different syscalls.

## kqueue to epoll, one to one

| kqueue (05) | epoll (this phase) |
|-------------|--------------------|
| `kqueue()` | `epoll_create1()` |
| `EV_SET(..., EVFILT_READ, EV_ADD)` | `epoll_ctl(EPOLL_CTL_ADD)` with `EPOLLIN` |
| `EV_SET(..., EV_DELETE)` | `epoll_ctl(EPOLL_CTL_DEL)` |
| `kevent(kq, NULL, 0, events, ...)` | `epoll_wait(epfd, events, ...)` |

## Build and run

**Linux only.** There is no `<sys/epoll.h>` on macOS, so this will not
compile there.

```sh
make
./server
```

On a Mac, run it in a container:

```sh
docker run --rm -it -v "$PWD/..":/src -w /src/06-epoll gcc:14 \
  sh -c "make && ./server"
```

```sh
nc localhost 9999
```

## Next

[07-framing](../07-framing) leaves the plain echo behind and builds a real
length-prefixed wire protocol with output buffering and backpressure, on
top of the kqueue loop from phase 05.
