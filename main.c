//
// Created by mahdi on 7/28/25.
//
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "protocol.h"
#include "bufpool.h"
#include "version.h"
#include "uthash.h"
#include "banner.h"
#include "utils.h"

#define LISTEN_ADDRESS "0.0.0.0"
#define LISTEN_PORT    9390

Buffer* pool = NULL;
uv_loop_t *loop = NULL;
uv_udp_t *udp_server = NULL;

typedef struct {
    conn_id_t key;
    uv_udp_t *client;
    uint64_t  last_seen_ns;
    UT_hash_handle hh;
} UniqueUDPHandle;

UniqueUDPHandle *client_map = NULL;

void on_close(uv_handle_t *handle) {
    backroute_t *br = (backroute_t *) handle->data;
    if (br) {
        free_backroute(br);
    }
    free(handle);
}

#define IDLE_NS     (40ULL * 1000 * 1000 * 1000)   /* 40 s */
#define BUDGET_NS   100000ULL                      /* ≤ 0.1 ms per slice */

typedef struct {
    uv_timer_t  t;
    UniqueUDPHandle *cursor;        /* resume point */
} gc_ctx_t;

static void gc_cb(uv_timer_t *h)
{
    gc_ctx_t *ctx = h->data;
    uint64_t  start = uv_hrtime();
    uint64_t  now   = start;

    UniqueUDPHandle *e = ctx->cursor ? ctx->cursor : client_map;
    while (e && (now = uv_hrtime()) - start < BUDGET_NS) {
        UniqueUDPHandle *next = e->hh.next;

        if (now - e->last_seen_ns > IDLE_NS) {
            LOG("removed from hash\n", "delete_gc");
            uv_close((uv_handle_t*)e->client, on_close);   /* cleanup client */
            HASH_DEL(client_map, e);
            free(e);
        }
        e = next;
    }
    ctx->cursor = e;                 /* NULL means table sweep complete */
}

void on_send(uv_udp_send_t *req, int status) {
    if (status < 0) {
        LOG("error: sending to udp: %s\n", "on_send", uv_strerror(status));
    }

    if (req) {
        if (req->data)
            put_buffer(&pool, (Buffer *) req->data);
        free(req);
    }
}

void on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    const Buffer *b = get_buffer(&pool);
    buf->base = b->data;
    buf->len = BUF_SIZE;
}

void on_upstream_recv(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr,
                      unsigned flags);

uv_udp_t *get_or_create_upstream_client(const struct sockaddr_in *src, const struct sockaddr_in *dest,
                                        uv_handle_t *source_handle) {
    conn_id_t key;
    make_connection_id(&key, src, dest); // fills in the struct

    UniqueUDPHandle *entry = NULL;
    HASH_FIND(hh, client_map, &key, sizeof(conn_id_t), entry);

    if (entry) {
        entry->last_seen_ns = uv_hrtime();
        return entry->client;
    }

    uv_udp_t *client = malloc(sizeof(uv_udp_t));
    if (!client) return NULL;

    int err = uv_udp_init(loop, client);
    if (err < 0) {
        free(client);
        return NULL;
    }

    // Reverse route
    backroute_t *br = new_backroute(source_handle, (const struct sockaddr *) src);
    client->data = br;

    err = uv_udp_recv_start(client, on_alloc, on_upstream_recv);
    if (err < 0) {
        free_backroute(br);
        free(client);
        return NULL;
    }

    entry = malloc(sizeof(UniqueUDPHandle));
    if (!entry) {
        free_backroute(br);
        uv_close((uv_handle_t *) client, NULL); // Cleanup safely
        return NULL;
    }

    memcpy(&entry->key, &key, sizeof(conn_id_t));
    entry->client = client;
    entry->last_seen_ns = uv_hrtime();
    HASH_ADD(hh, client_map, key, sizeof(conn_id_t), entry);

    return client;
}

void on_upstream_recv(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr,
                      unsigned flags) {
    int uv_error = 0;
    backroute_t *br = NULL;

    if (nread < 0) {
        LOG("error: failed to receive: %s\n", "on_upstream_recv", uv_strerror((int)nread));
        goto cleanup;
    }

    if (handle->data == NULL) {
        LOG("error: invalid packet, dropping\n", "on_upstream_recv");
        goto cleanup;
    }

    if (addr == NULL)
        goto cleanup;

    br = (backroute_t *) handle->data;

    const struct sockaddr_in *addr_in = (const struct sockaddr_in *) addr;
    char ip[15] = {0};
    //uv_error = uv_ip4_name(addr_in, ip, sizeof(ip));
    //if (uv_error < 0) {
//        LOG("error: failed to convert address to string: %s\n", "on_upstream_recv", uv_strerror(uv_error));
    //    goto cleanup;
  //  }

    char dest_ip[15] = {0};
    //uv_error = uv_ip4_name((const struct sockaddr_in *) br->source, dest_ip, sizeof(ip));
    //if (uv_error < 0) {
     //   LOG("error: failed to convert address to string: %s\n", "on_upstream_recv", uv_strerror(uv_error));
     //   goto cleanup;
    //}

#ifdef _DEBUG
    char* hex_payload = to_hex_string(buf->base, nread > 32 ? (size_t) 32 : nread);
    LOG("received new packet from upstream:\n \
	- Sender: %s:%d\n \
	- Payload Length: %ld\n \
	- Needed to forward to: %s:%d\n \
	- First 32 bytes: %s\n", "on_upstream_recv", ip, htons(addr_in->sin_port), nread, dest_ip,
        htons(((const struct sockaddr_in*)br->source)->sin_port), hex_payload);
    free(hex_payload);
#endif

    const char *destination_info = convert_destination_to_payload((const struct sockaddr_in *) addr);
    if (destination_info == NULL) {
        LOG("error: failed to create payload from sockaddr\n", "on_upstream_recv");
        goto cleanup;
    }

    char *real_payload = get_buffer(&pool)->data;
    if (real_payload == NULL) {
        LOG("error: failed to allocate real_payload\n", "on_upstream_recv");
        goto cleanup;
    }

    memcpy(real_payload, destination_info, 6);
    memcpy(real_payload + 6, buf->base, nread);
    free((void *) destination_info);

    uv_buf_t upstream_buffer = uv_buf_init(real_payload, (unsigned int) nread + 6);

    // uv_udp_send_t *send_request = (uv_udp_send_t *) malloc(sizeof(uv_udp_send_t));
    // if (send_request == NULL) {
    //     LOG("error: failed to allocate uv_udp_send_t\n", "on_upstream_recv");
    //     goto cleanup;
    // }
    // send_request->data = real_payload;

    uv_udp_try_send((uv_udp_t*)br->source_handle, &upstream_buffer, 1, br->source);
    //uv_error = uv_udp_send(send_request, (uv_udp_t *) br->source_handle, &upstream_buffer, 1,
    //                       (const struct sockaddr *) br->source, on_send);
    //if (uv_error < 0) {
    //    LOG("error: failed to send payload to upstream: %s\n", "on_upstream_recv", uv_strerror(uv_error));
    //}
    put_buffer(&pool, (Buffer *) real_payload);

cleanup:
    if (buf->base)
        put_buffer(&pool, (Buffer *) buf->base);
}

void on_recv(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr*addr, unsigned flags) {
    int uv_error = 0;
    if (nread < 0) {
        LOG("error: failed to receive: %s\n", "on_recv", uv_strerror((int)nread));
        goto cleanup;
    }

    if (addr == NULL)
        goto cleanup;

    if (nread < 6) {
        LOG("error: invalid packet, dropping\n", "on_recv");
        goto cleanup;
    }

    const struct sockaddr_in *addr_in = (const struct sockaddr_in *) addr;
    char ip[15] = {0};
    //uv_error = uv_ip4_name(addr_in, ip, sizeof(ip));
    //if (uv_error < 0) {
    //    LOG("error: failed to convert address to string: %s\n", "on_recv", uv_strerror(uv_error));
    //    goto cleanup;
    //}

    const struct sockaddr_in server_addr = convert_payload_to_destination(buf->base);
    char dest_ip[15] = {0};
    //uv_error = uv_ip4_name(&server_addr, dest_ip, sizeof(dest_ip));
    //if (uv_error < 0) {
    //    LOG("error: failed to convert address to string: %s\n", "on_recv", uv_strerror(uv_error));
    //    goto cleanup;
    //}

    char *payload = buf->base + 6;
    nread -= 6;

#ifdef _DEBUG
    char* hex_payload = to_hex_string(payload, nread > 32 ? 32 : nread);
    LOG("received new packet:\n"
	"- Sender: %s:%d\n"
	"- Payload Length: %ld\n"
	"- Needed Destination: %s:%d\n"
	"- First 32 bytes: %s\n", "on_recv", ip, htons(addr_in->sin_port), nread, dest_ip, htons(server_addr.sin_port),
        hex_payload);
    free(hex_payload);
#endif

    uv_udp_t *upstream = get_or_create_upstream_client((const struct sockaddr_in *) addr, &server_addr,
                                                       (uv_handle_t *) handle);
    if (!upstream) {
        LOG("error: failed to get upstream client\n", "on_recv");
        goto cleanup;
    }


    char *real_payload = get_buffer(&pool)->data;
    memcpy(real_payload, payload, nread);

    uv_buf_t upstream_buffer = uv_buf_init(real_payload, (unsigned int) nread);
    // uv_udp_send_t *send_request = (uv_udp_send_t *) malloc(sizeof(uv_udp_send_t));
    // if (send_request == NULL) {
    //     LOG("error: failed to allocate uv_udp_send_t\n", "on_recv");
    //     goto cleanup;
    // }
    // send_request->data = real_payload;

    uv_udp_try_send(upstream, &upstream_buffer, 1, (const struct sockaddr *)&server_addr);
    // uv_error = uv_udp_send(send_request, upstream, &upstream_buffer, 1, (const struct sockaddr *) &server_addr,
    //                        on_send);
    // if (uv_error < 0) {
    //     LOG("error: failed to send payload to upstream: %s\n", "on_recv", uv_strerror(uv_error));
    // }
    put_buffer(&pool, (Buffer *) real_payload);


cleanup:
    if (buf->base)
        put_buffer(&pool, (Buffer *) buf->base);
}

void signal_handler(uv_signal_t *handle, int signum) {
    LOG("received signal %d, shutting down...\n", "signal_handler", signum);
    uv_close((uv_handle_t *) udp_server, on_close);
    udp_server = NULL;

    uv_stop(loop);
}

int main(void) {
    printf(BANNER, VERSION, RELEASE_DATE, AUTHOR);

    loop = uv_default_loop();
    int uv_error = 0;

    pool = init_pool(NULL);
    uv_signal_t sig;

    LOG("Application started\n", "main");

    struct sockaddr_in listen_address;
    uv_error = uv_ip4_addr(LISTEN_ADDRESS, LISTEN_PORT, &listen_address);
    if (uv_error < 0) {
        LOG("error: filling listen_address: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

    udp_server = (uv_udp_t *) malloc(sizeof(uv_udp_t));
    if (udp_server == NULL) {
        LOG("error: failed to allocate uv_udp_t\n", "main");
        uv_error = -1; // forces app to close with 1 error code.
        goto cleanup;
    }

    uv_error = uv_udp_init(loop, udp_server);
    if (uv_error < 0) {
        LOG("error: initializing udp handle: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

    uv_error = uv_udp_bind(udp_server, (const struct sockaddr *) &listen_address, 0);
    if (uv_error < 0) {
        LOG("error: binding on udp handle: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

    uv_error = uv_udp_recv_start(udp_server, on_alloc, on_recv);
    if (uv_error < 0) {
        LOG("error: start receiving on udp handle: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

    uv_error = uv_signal_init(loop, &sig);
    if (uv_error < 0) {
        LOG("error: initializing signal handler: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

    uv_error = uv_signal_start(&sig, signal_handler, SIGINT);
    if (uv_error < 0) {
        LOG("error: starting signal handler: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

    gc_ctx_t *gc = calloc(1, sizeof *gc);
    uv_timer_init(loop, &gc->t);
    gc->t.data = gc;
    uv_timer_start(&gc->t, gc_cb, 1000, 1);

    uv_error = uv_run(loop, UV_RUN_DEFAULT);
    if (uv_error < 0) {
        LOG("error: starting running event loop: %s\n", "main", uv_strerror(uv_error));
        goto cleanup;
    }

cleanup:
    if (udp_server)
        uv_close((uv_handle_t *) udp_server, on_close);

    if (uv_error < 0) return 1;
    return 0;
}
