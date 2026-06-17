#include "net.h"
#include <stdlib.h>
#include <string.h>

/* Implemented in parser.c: parse one command line + execute, calling reply(). */
extern void parse_and_execute(struct work_req *w);

#define READ_CHUNK      4096
#define RBUF_INIT_CAP   4096
#define RBUF_MAX_CAP    (64 * 1024) /* backpressure cap; over-long line drops conn */
#define RESP_INIT_CAP   64

__thread struct work_req *current_work = NULL;

static uv_tcp_t listen_handle;

static void on_connection(uv_stream_t *server, int status);
static void alloc_cb(uv_handle_t *h, size_t suggested, uv_buf_t *buf);
static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
static void on_close(uv_handle_t *h);
static void on_write(uv_write_t *req, int status);
static void work_cb(uv_work_t *req);
static void after_work_cb(uv_work_t *req, int status);
static void dispatch_line(struct conn_t *c, const char *line, size_t llen);
static void try_dispatch_next(struct conn_t *c);
static void maybe_close(struct conn_t *c);

/* ---- reply: append into current work's resp_buf (called from work thread) ---- */
void reply(const char *s, size_t len)
{
    struct work_req *w = current_work;
    if (!w)
    {
        return;
    }
    if (w->resp_len + len > w->resp_cap)
    {
        while (w->resp_len + len > w->resp_cap)
        {
            w->resp_cap *= 2;
        }
        w->resp_buf = realloc(w->resp_buf, w->resp_cap);
    }
    memcpy(w->resp_buf + w->resp_len, s, len);
    w->resp_len += len;
}

int run_server(uv_loop_t *loop, struct sockaddr_in *addr)
{
    int r;
    uv_tcp_init(loop, &listen_handle);
    if ((r = uv_tcp_bind(&listen_handle, (const struct sockaddr *)addr, 0)))
    {
        LOG("uv_tcp_bind failed: %s", uv_strerror(r));
        return r;
    }
    if ((r = uv_listen((uv_stream_t *)&listen_handle, 128, on_connection)))
    {
        LOG("uv_listen failed: %s", uv_strerror(r));
        return r;
    }
    return 0;
}

static void maybe_close(struct conn_t *c)
{
    /* only close once the in-flight work is done (busy==0) so libuv never
     * aborts on a uv_close with a pending uv_write */
    if (c->closing && !c->busy)
    {
        uv_close((uv_handle_t *)&c->handle, on_close);
    }
}

static void on_connection(uv_stream_t *server, int status)
{
    struct conn_t *c;
    if (status < 0)
    {
        LOG("on_connection: %s", uv_strerror(status));
        return;
    }
    c = calloc(1, sizeof(struct conn_t));
    if (!c)
    {
        return;
    }
    c->loop = server->loop;
    c->rcap = RBUF_INIT_CAP;
    c->rbuf = malloc(c->rcap);
    c->rlen = 0;
    c->busy = 0;
    c->closing = 0;
    uv_tcp_init(c->loop, &c->handle);
    c->handle.data = c;
    if (uv_accept(server, (uv_stream_t *)&c->handle) == 0)
    {
        uv_read_start((uv_stream_t *)&c->handle, alloc_cb, on_read);
    }
    else
    {
        uv_close((uv_handle_t *)&c->handle, on_close);
    }
}

static void alloc_cb(uv_handle_t *h, size_t suggested, uv_buf_t *buf)
{
    (void)h;
    (void)suggested;
    buf->base = malloc(READ_CHUNK);
    buf->len = READ_CHUNK;
}

static void dispatch_line(struct conn_t *c, const char *line, size_t llen)
{
    struct work_req *w = calloc(1, sizeof(struct work_req));
    if (!w)
    {
        return;
    }
    w->conn = c;
    w->line = malloc(llen + 1);
    memcpy(w->line, line, llen);
    w->line[llen] = '\0';
    w->line_len = llen;
    w->resp_cap = RESP_INIT_CAP;
    w->resp_buf = malloc(w->resp_cap);
    w->resp_len = 0;
    c->busy = 1;
    uv_queue_work(c->loop, &w->req, work_cb, after_work_cb);
}

static void on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    struct conn_t *c = stream->data;
    if (nread < 0)
    {
        free(buf->base);
        c->closing = 1;
        maybe_close(c);
        return;
    }
    if (nread == 0)
    {
        free(buf->base);
        return;
    }

    /* append to framing buffer, grow if needed (cap = backpressure) */
    if (c->rlen + (size_t)nread > c->rcap)
    {
        size_t ncap = c->rcap;
        while (ncap < c->rlen + (size_t)nread)
        {
            ncap *= 2;
        }
        if (ncap > RBUF_MAX_CAP)
        {
            free(buf->base);
            c->closing = 1;
            maybe_close(c);
            return;
        }
        c->rbuf = realloc(c->rbuf, ncap);
        c->rcap = ncap;
    }
    memcpy(c->rbuf + c->rlen, buf->base, nread);
    c->rlen += nread;
    free(buf->base);

    /* dispatch the first complete line; serial execution keeps replies ordered */
    size_t scan = 0;
    while (!c->busy)
    {
        char *nl = memchr(c->rbuf + scan, '\n', c->rlen - scan);
        if (!nl)
        {
            break; /* half packet, wait for more */
        }
        size_t llen = nl - (c->rbuf + scan);
        dispatch_line(c, c->rbuf + scan, llen); /* sets c->busy = 1 */
        scan += llen + 1;
    }
    if (scan)
    {
        memmove(c->rbuf, c->rbuf + scan, c->rlen - scan);
        c->rlen -= scan;
    }
}

static void try_dispatch_next(struct conn_t *c)
{
    char *nl;
    size_t llen, scan;
    if (c->closing)
    {
        maybe_close(c);
        return;
    }
    nl = memchr(c->rbuf, '\n', c->rlen);
    if (!nl)
    {
        return; /* no complete line yet; wait for on_read */
    }
    llen = nl - c->rbuf;
    dispatch_line(c, c->rbuf, llen); /* copies the line, sets busy=1 */
    scan = llen + 1;
    memmove(c->rbuf, c->rbuf + scan, c->rlen - scan);
    c->rlen -= scan;
}

static void work_cb(uv_work_t *req)
{
    struct work_req *w = (struct work_req *)req;
    current_work = w;
    parse_and_execute(w);
    current_work = NULL;
}

static void after_work_cb(uv_work_t *req, int status)
{
    struct work_req *w = (struct work_req *)req;
    struct conn_t *c = w->conn;
    uv_buf_t ub;
    c->busy = 0;
    if (status < 0 || c->closing || w->resp_len == 0)
    {
        free(w->line);
        free(w->resp_buf);
        free(w);
        maybe_close(c);
        return;
    }
    w->wr_req.data = w;
    ub = uv_buf_init(w->resp_buf, w->resp_len);
    if (uv_write(&w->wr_req, (uv_stream_t *)&c->handle, &ub, 1, on_write))
    {
        free(w->line);
        free(w->resp_buf);
        free(w);
        c->closing = 1;
        maybe_close(c);
    }
}

static void on_write(uv_write_t *req, int status)
{
    struct work_req *w = req->data;
    struct conn_t *c = w->conn;
    free(w->line);
    free(w->resp_buf);
    free(w);
    if (status < 0)
    {
        c->closing = 1;
        maybe_close(c);
        return;
    }
    /* reply flushed in order; now dispatch the next buffered line for this conn */
    try_dispatch_next(c);
}

static void on_close(uv_handle_t *h)
{
    struct conn_t *c = h->data;
    free(c->rbuf);
    free(c);
}
