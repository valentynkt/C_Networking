# Design: building-tcp-servers-in-c

**Date:** 2026-06-24
**Status:** Approved (pending spec review)

## Goal

Refactor the `C_Networking` repo into a state-of-the-art learning resource: one
TCP echo server, evolved across 7 self-contained phases from a blocking
one-client-at-a-time server to an event-loop server with a length-prefixed wire
protocol. Each phase stands alone, reads top-to-bottom, and carries its own
README so others can learn from it and the author can mine it for an article
series.

The narrative spine is the **C10K problem**: each phase exposes a concrete pain,
and the next phase fixes it.

## Decisions (locked)

| # | Decision | Choice |
|---|----------|--------|
| 1 | Repo name | `building-tcp-servers-in-c` |
| 2 | Shared code | Keep a small shared `common/` module (socket boilerplate). Only `event_loop` is decoupled. |
| 3 | Build | Per-folder Makefile; each folder builds independently. No root build target. |
| 4 | Scope | Refine existing 5 phases + add `poll()` + Linux `epoll` mirror → 7 phases. |
| 5 | Headers | `server.c` only per phase; a `.h` only where it earns its place (framing's `frame.h`). |
| 6 | Echo format | Pure echo — drop `format_response`/`PREFIX`/`SUFFIX` from common. |

## Final structure

```
building-tcp-servers-in-c/
├── README.md            # the journey: C10K arc, phase table, build/run, the shared module
├── common/
│   ├── common.h         # create_listener, write_all, set_non_blocking + constants
│   ├── common.c
│   └── README.md        # what's shared and WHY event_loop is deliberately NOT here
├── 01-blocking/         # server.c · Makefile · README.md
├── 02-nonblocking/
├── 03-select/
├── 04-poll/             # NEW
├── 05-kqueue/           # event loop INLINED (raw kqueue), macOS/BSD
├── 06-epoll/            # NEW, Linux mirror of kqueue
└── 07-framing/          # server.c · frame.h · Makefile · README.md · test_framing.py
docs/superpowers/specs/   # this design doc
.gitignore
.claude/settings.json     # clangd-lsp plugin (kept)
```

## The 7 phases

Each phase echoes back what it receives. Pain → fix narrative:

| # | Phase | Mechanism | Pain it exposes (motivates next) |
|---|-------|-----------|----------------------------------|
| 01 | blocking | `accept`+`read` block | 2nd client hangs until 1st disconnects |
| 02 | nonblocking | `O_NONBLOCK` busy-poll | works for N clients, but 100% CPU when idle |
| 03 | select | `select()` multiplexing | CPU ~0% idle, but `FD_SETSIZE` cap + rebuild fd_set each loop + O(n) scan |
| 04 | poll | `poll()` + `struct pollfd[]` | no FD_SETSIZE cap, cleaner API, but still O(n) scan of all fds |
| 05 | kqueue | inlined kqueue loop | O(ready) not O(n), register once. macOS/BSD only |
| 06 | epoll | inlined epoll loop | same rung as kqueue, Linux kernel. Linux only |
| 07 | framing | kqueue loop + length-prefix protocol + per-client write buffers | TCP is a byte stream, not messages — need framing + backpressure |

**kqueue ↔ epoll** are the *same conceptual rung* on two kernels, cross-referenced
in both READMEs — epoll is not "more advanced than" kqueue.

## Conventions

### common/ module
- `common.h` / `common.c`: `create_listener(void)`, `write_all(fd, buf, len)`,
  `set_non_blocking(fd)`, and constants (`PORT`, `BACKLOG`, `BUF_SIZE`,
  `MAX_CLIENTS`).
- **Removed:** `format_response`, `PREFIX`, `SUFFIX` — pure echo across all phases.
- `common/README.md` states the deliberate boundary: socket boilerplate is shared
  because it's identical and not the lesson; the event loop is *not* shared because
  watching it grow per-phase IS the lesson.

### event_loop — deleted
`src/event_loop.c/h` is removed. Phases 05/06/07 each inline their own readiness
loop (raw `kevent`/`epoll_wait` register + dispatch) so each file reads
top-to-bottom with the syscalls visible in context. `07-framing` inlines a kqueue
loop (matches dev machine; README notes epoll is identical).

### Per-folder Makefile
```make
CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -std=c17 -g -fsanitize=address,undefined
server: server.c ../common/common.c
	$(CC) $(CFLAGS) -I../common -o server server.c ../common/common.c
clean:
	rm -f server
	rm -rf server.dSYM
.PHONY: clean
```
- `epoll` Makefile: same shape; README says "Linux only — won't compile on macOS."
- Each folder independently buildable: `cd 04-poll && make && ./server`.

### Per-phase README template
1. **What it does** — one line.
2. **The problem it solves** — what the previous phase couldn't do.
3. **Feel the pain** — concrete reproduction of THIS phase's limitation (e.g.
   "run `top` while idle", "connect 2 clients", "fd > FD_SETSIZE").
4. **Key syscalls** — the 2-4 calls that matter, one line each.
5. **Build & run** — `make && ./server`, port 9999.
6. **Try it** — `nc localhost 9999` / telnet / `test_framing.py` commands.
7. **Next** — which phase comes next and *why* (the pain above).

### Top-level README
- Hook: the C10K problem.
- Phase table (the one above) with links to each folder.
- "How to read this repo" — clone, pick a phase, `make`, read `server.c`
  top-to-bottom alongside its README.
- The shared `common/` module explained + why `event_loop` is intentionally not shared.
- Platform note: 01-04 portable POSIX; 05 macOS/BSD; 06 Linux; 07 macOS/BSD.

## Code changes per phase

- **01 blocking, 02 nonblocking, 03 select:** relocate to numbered folders, rename
  to `server.c`, fold the `run_*_server()` body into `main()`, drop the per-phase
  `.h`, switch to pure echo (`write_all(fd, buf, n)` instead of `format_response`).
- **04 poll (new):** mirror select's structure with a `struct pollfd fds[]` array.
  Compact-on-disconnect. Pure echo. Demonstrates no FD_SETSIZE cap, still O(n).
- **05 kqueue:** inline the deleted event_loop's kqueue logic directly into
  `main()`/handlers. Raw `kqueue()`, `EV_SET`, `kevent()`. Pure echo.
- **06 epoll (new):** Linux mirror of 05 — `epoll_create1`, `epoll_ctl`,
  `epoll_wait`, `struct epoll_event`. Pure echo. Documented Linux-only.
- **07 framing:** keep the length-prefix protocol + per-client read/write buffers +
  backpressure logic. Inline its own kqueue loop (no shared event_loop). Protocol
  constants/`client_t` struct go in `frame.h`. Move `test_framing.py` into this folder.

## Rename

- `git mv` source files into new folders (preserve history where practical).
- Rename local dir `C_Networking` → `building-tcp-servers-in-c`.
- Rename the GitHub repo `C_Networking` → `building-tcp-servers-in-c` (via `gh repo rename`).
- Update `origin` remote URL. Note: current remote points at user `valentynkt`;
  the account was renamed to `valentynkit` — verify and set the correct URL.

## Verification

- Build every folder: `for d in 0*/ ; do (cd "$d" && make) || exit 1; done`
  (run on macOS; `06-epoll` expected to fail to compile there — verify on a Linux
  container or note as untestable locally).
- Smoke-test 01-05 + 07 on macOS: start server, `nc`/telnet round-trip, check echo.
- `07-framing`: run `python3 test_framing.py` (all 5 tests pass).
- ASan/UBSan clean (already in CFLAGS) under the smoke tests.

## Authorship & style (hard constraint)

Everything committed must read as written by a human developer, not generated:

- **Commits:** plain, factual subject lines in the author's existing voice (see
  `git log` history). No emoji, no "Co-Authored-By: Claude" footer, no AI-tell
  phrasing ("comprehensive", "robust", "seamlessly"), no em dashes.
- **Code comments:** keep the pedagogical ones (they explain *why* / the pain),
  cut noise (no comment that restates the line below it). Match the density already
  in the codebase, which is good.
- **READMEs:** normal prose, no em dashes, no marketing tone, no emoji-bullets.
  Write like a developer explaining to a peer.
- No "AI slop": no filler, no over-hedging, no restating the obvious.

## Out of scope

- TLS, HTTP, threading/forking models, io_uring.
- Windows/IOCP.
- A root "build-all" target (per-folder by decision #3).
