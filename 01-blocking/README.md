# 01 - blocking

One client at a time. The starting point.

## The idea

A bare TCP server: `accept()` a connection, then `read()` and echo in a
loop until the client leaves. Both calls block, meaning they put the
process to sleep until there is work to do. That is fine for a single
client and a disaster for two.

## Key calls

- `accept()` blocks until a client connects.
- `read()` blocks until that client sends bytes.
- `write_all()` echoes them straight back.

## Build and run

```sh
make
./server
```

Listens on port 9999.

## Feel the problem

Open two terminals and connect both:

```sh
nc localhost 9999    # terminal A
nc localhost 9999    # terminal B
```

Type in terminal A and you get echoes. Type in terminal B and nothing
happens. The server is stuck inside `handle_client()` for A and will not
touch B until A disconnects. One slow client freezes everyone.

## Next

[02-nonblocking](../02-nonblocking) breaks the freeze by refusing to let
any single call block, so the server can juggle every client in one loop.
