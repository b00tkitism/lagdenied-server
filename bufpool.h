//
// Created by mahdi on 7/28/25.
//

#ifndef BUFPOOL_H
#define BUFPOOL_H
#define BUF_POOL_SIZE 100
#define BUF_SIZE 65536

typedef struct Buffer {
    char data[BUF_SIZE];
    struct Buffer* next;
} Buffer;

Buffer* init_pool(Buffer* pool);
Buffer* get_buffer(Buffer** pool);
void put_buffer(Buffer** pool, Buffer* b);
#endif //BUFPOOL_H
