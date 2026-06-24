#ifndef FRAME_H
#define FRAME_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Wire format: [4-byte big-endian length N][N bytes of payload].
 *
 * TCP is a byte stream with no message boundaries. One read() can hand you
 * half a message, or three messages glued together. Prefixing each message
 * with its length lets the server cut the stream back into whole messages.
 *
 * This header holds the protocol constants and the per-client state. The
 * logic that uses them lives in server.c.
 */

#define MAX_CLIENTS_FD 1024            /* index clients by fd, so size to fd count */
#define MSG_MAX        4096            /* largest payload we accept */
#define FRAME_HDR_SIZE 4               /* the length prefix, a uint32 */
#define WBUF_SIZE      ((MSG_MAX + FRAME_HDR_SIZE) * 2)

typedef struct {
    bool   active;
    char   buf[MSG_MAX + FRAME_HDR_SIZE];  /* read buffer: raw bytes so far */
    size_t len;                            /* bytes accumulated in buf */
    char   wbuf[WBUF_SIZE];                /* write buffer: queued response bytes */
    size_t wlen;                           /* total bytes queued in wbuf */
    size_t woff;                           /* bytes of wbuf already sent */
} client_t;

#endif
