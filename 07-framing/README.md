# 07 - framing

A real wire protocol on the event loop. macOS and BSD.

## The idea

Every phase up to here echoed raw bytes. That hides the fact that TCP has
no message boundaries: a single `read()` can return half a message, or
three messages stuck together. A real server has to frame the stream.

The wire format is dead simple (see `frame.h`):

```
[4-byte big-endian length N][N bytes payload]
```

The read side accumulates bytes in a per-client buffer and pulls out
complete frames. The write side does not just blast the response back, it
queues it and drains it when the kernel says the socket is writable. This
phase reuses the inlined kqueue loop from phase 05 and adds the two things
that turn an echo toy into a server.

## The two real problems it solves

- **Framing.** `process_buffer()` reads the 4-byte length, waits until the
  whole payload has arrived, then echoes the complete frame. Pipelined
  messages and partial reads both just work.
- **Backpressure.** `queue_write()` buffers output per client and registers
  `EVFILT_WRITE`. `on_writable()` drains it. If a slow client lets the
  write buffer fill, `process_buffer()` stops pulling new frames until the
  buffer drains, so one slow reader cannot make the server allocate without
  limit. A payload larger than `MSG_MAX` is treated as a protocol violation
  and the client is dropped.

## Key calls

- `EVFILT_READ` for incoming data, `EVFILT_WRITE` only while output is
  pending.
- `ntohl()` to read the big-endian length prefix.
- `memmove()` to compact consumed bytes out of the read and write buffers.

## Build and run

```sh
make
./server
```

## Try it

`nc` is awkward here because the protocol is binary, so there is a Python
client:

```sh
python3 test_framing.py            # run all tests
python3 test_framing.py pipelining # run one by name
```

It covers a single message, several messages on one connection, three
frames pipelined into one send, a payload near `MSG_MAX`, and an oversized
payload that the server must reject.

## Where to go next

This is the end of the planned arc. Real next steps if you keep going:
a thread or process per core, `io_uring` on Linux, TLS, or a higher-level
protocol (HTTP) on top of this framing.
