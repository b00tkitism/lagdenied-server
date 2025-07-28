//
// Created by mahdi on 7/28/25.
//
#include <stdlib.h>
#include "bufpool.h"

#include <string.h>

Buffer* init_pool(Buffer* pool) {
    for (int i = 0; i < BUF_POOL_SIZE; ++i) {
        Buffer* b = malloc(sizeof(Buffer));
        b->next = pool;
        pool = b;
    }

    return pool;
}

Buffer* get_buffer(Buffer** pool) {
    Buffer* out = *pool;

    if (!*pool) {
        return NULL;
    }
    *pool = out->next;
    return out;
}

void put_buffer(Buffer** pool, Buffer* b) {
    b->next = *pool;
    *pool = b;
}