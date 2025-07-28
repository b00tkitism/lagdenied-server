//
// Created by mahdi on 7/28/25.
//

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "uv.h"

#pragma pack(push, 1)
typedef struct {
    uint32_t src_ip;
    uint16_t src_port;
    uint32_t dest_ip;
    uint16_t dest_port;
} conn_id_t;
#pragma pack(pop)

void make_connection_id(conn_id_t* id, const struct sockaddr_in* src, const struct sockaddr_in* dest);

typedef struct backroute_s {
    uv_handle_t* source_handle;
    struct sockaddr* source;
} backroute_t;

backroute_t* new_backroute(uv_handle_t* source_handle, const struct sockaddr* source);
void free_backroute(backroute_t* br);
#endif //PROTOCOL_H