#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "blocking.h"
#include "nonblocking.h"
#include "select_server.h"
#include "kqueue_server.h"
#include "framing.h"

static void usage(const char *prog)
{
    fprintf(stderr, "Usage: %s <mode>\n", prog);
    fprintf(stderr, "Modes:\n");
    fprintf(stderr, "  blocking     Phase 1: one client at a time\n");
    fprintf(stderr, "  nonblocking  Phase 2: busy-poll, 100%% CPU\n");
    fprintf(stderr, "  select       Phase 3: kernel-multiplexed\n");
    fprintf(stderr, "  kqueue       Phase 4: event loop\n");
    fprintf(stderr, "  framing      Phase 5: length-prefixed wire protocol\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    const char *mode = argv[1];

    if (strcmp(mode, "blocking") == 0)
        return run_blocking_server();

    if (strcmp(mode, "nonblocking") == 0)
        return run_nonblocking_server();

    if (strcmp(mode, "select") == 0)
        return run_select_server();

    if (strcmp(mode, "kqueue") == 0)
        return run_kqueue_server();

    if (strcmp(mode, "framing") == 0)
        return run_framing_server();

    fprintf(stderr, "Unknown mode: %s\n", mode);
    usage(argv[0]);
    return EXIT_FAILURE;
}
