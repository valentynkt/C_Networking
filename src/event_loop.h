#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

typedef struct event_loop event_loop_t;
typedef void (*el_handler_fn)(event_loop_t *el, int fd);

struct event_loop {
    int kq;
    int server_fd;
    el_handler_fn on_accept;
    el_handler_fn on_read;
    void *ctx;
};

int  el_init(event_loop_t *el, int server_fd,
             el_handler_fn on_accept, el_handler_fn on_read,
             void *ctx);
int  el_register_read(event_loop_t *el, int fd);
void el_remove_fd(event_loop_t *el, int fd);
int  el_run(event_loop_t *el);
void el_cleanup(event_loop_t *el);

#endif
