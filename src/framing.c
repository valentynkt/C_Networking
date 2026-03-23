#include "framing.h"
#include "common.h"
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/time.h>
#include <sys/types.h>

#define MAX_CLIENTS_FD 1024
#define MSG_MAX 4096
#define FRAME_HDR_SIZE 4

/*
 * Phase 5: Wire Protocol — Length-Prefixed Framing (kqueue)
 *
 * Same kqueue event loop as Phase 4. What changes: how we interpret bytes.
 *
 * Wire format:
 *   [4 bytes: payload length, big-endian uint32][payload bytes]
 *
 * TCP is a byte stream — one read() may return:
 *   - Half a length prefix
 *   - A full message + half of the next
 *   - 3 complete messages concatenated
 *
 * Your job: accumulate bytes per client, extract complete framed
 * messages, echo them back with the same framing.
 *
 * Key syscalls/functions you'll need:
 *   htonl() / ntohl()  — host <-> network byte order
 *   memcpy()           — extract length prefix from buffer
 *   memmove()          — compact buffer after extracting a message
 */

/* TODO: define per-client state struct
 *
 * Think about what each client needs:
 *   - Which fd does this client own?
 *   - Where do you accumulate partial reads?
 *   - How do you know how many bytes are in the buffer?
 *   - Is this slot in use?
 */

/* TODO: define server state struct
 *
 * Same idea as Phase 4's server_t, but now clients carry state
 * beyond just "connected or not".
 */

/* TODO: implement
 *
 * Functions to build (suggested, not required — structure it how you want):
 *
 *   init_server()      — kqueue setup, same as Phase 4
 *   shutdown_server()   — cleanup all clients and fds
 *   accept_client()    — accept + register with kqueue + init client state
 *   remove_client()    — close fd + reset client state
 *   handle_client()    — read bytes, accumulate, extract framed messages, echo
 *   process_buffer()   — the framing state machine: do we have 4 bytes?
 *                         do we have length + payload? extract, compact, repeat
 *   send_framed()      — write [4-byte length][payload] to client
 */

int run_framing_server(void)
{
    /* TODO: your implementation here
     *
     * Structure:
     *   1. create_listener() + set_non_blocking()
     *   2. init kqueue, register server_fd
     *   3. Event loop:
     *      - kevent() blocks until ready fds
     *      - server_fd ready → accept_client()
     *      - client_fd ready → read into client's buffer → process_buffer()
     */
    return EXIT_FAILURE;
}
