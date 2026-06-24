# 02 - nonblocking

Many clients at once, but the CPU never rests.

## The idea

Set every fd to `O_NONBLOCK`. Now `accept()` and `read()` return right
away with `EAGAIN` when there is nothing to do, instead of sleeping. One
flat loop tries to accept a new connection, then walks the list of known
clients and tries to read from each. No single client can block the
others, so the freeze from phase 01 is gone.

## Key calls

- `set_non_blocking()` on every fd so calls never sleep.
- `EAGAIN` / `EWOULDBLOCK` means "nothing ready right now, move on."
- Disconnect handling uses swap-with-last to keep the client array dense.

## Build and run

```sh
make
./server
```

## Feel the problem

Start the server and, with no clients connected at all, watch its CPU:

```sh
top -pid $(pgrep -n server)
```

It sits at ~100% of a core while doing nothing. The loop spins forever,
asking the kernel "anything yet?" millions of times a second and getting
`EAGAIN` every time. Correct, but it would melt a real machine.

## Next

[03-select](../03-select) hands the waiting back to the kernel. `select()`
sleeps until at least one fd is actually ready, so an idle server uses
roughly zero CPU.
