#ifndef SOLIDBITS_NET_H
#define SOLIDBITS_NET_H

#include <uv.h>
#include <stddef.h>
#include "solidbits.h"

/* Per-connection context. Attached to uv_tcp_t.data. */
struct conn_t
{
    uv_tcp_t   handle;
    uv_loop_t *loop;
    char      *rbuf;    /* line-framing read buffer */
    size_t     rlen;
    size_t     rcap;
    int        busy;    /* 1 = a work_req is in flight for this connection */
    int        closing; /* 1 = peer closed or shutting down; drop further replies */
};

/* One framed command line, dispatched to the libuv thread pool.
 * req must be the first field so uv_work_t* <-> work_req* casts are valid. */
struct work_req
{
    uv_work_t      req;
    struct conn_t *conn;
    char          *line;      /* input: one command line, no trailing '\n' */
    size_t         line_len;
    char          *resp_buf;  /* output: accumulated reply text */
    size_t         resp_len;
    size_t         resp_cap;
    int            oom;       /* set by reply() if resp_buf could not grow */
    uv_write_t     wr_req;
    struct job     job;       /* parsed command + desc references */
};

/* Points at the work_req currently being executed on this work thread.
 * Set by net.c::work_cb; consumed by reply(). */
extern __thread struct work_req *current_work;

/* Append reply text to current_work->resp_buf (called from work thread). */
void reply(const char *s, size_t len);

/* Bind + listen a TCP server on addr. Returns 0 on success. */
int run_server(uv_loop_t *loop, struct sockaddr_in *addr);

#endif /* SOLIDBITS_NET_H */
