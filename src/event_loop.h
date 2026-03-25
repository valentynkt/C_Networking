#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H
#define MAX_FDS 1024

typedef struct event_loop event_loop_t;
typedef void (*el_handler_fn)(event_loop_t *el, int fd);

struct event_loop {
    int kq;
    el_handler_fn read_handlers[MAX_FDS];
    el_handler_fn write_handlers[MAX_FDS];
    void *ctx;
};

int  el_init(event_loop_t *el, void *ctx);
int  el_add(event_loop_t *el, int fd, el_handler_fn handler);
int  el_add_write(event_loop_t *el, int fd, el_handler_fn handler);
void el_remove(event_loop_t *el, int fd);
void el_remove_write(event_loop_t *el, int fd);
int  el_run(event_loop_t *el);
void el_cleanup(event_loop_t *el);

#endif
