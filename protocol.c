//
// Created by mahdi on 7/28/25.
//
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include "protocol.h"

void make_connection_id(conn_id_t* id, const struct sockaddr_in* src, const struct sockaddr_in* dest) {
    id->src_ip = src->sin_addr.s_addr;
    id->src_port = src->sin_port;
    id->dest_ip = dest->sin_addr.s_addr;
    id->dest_port = dest->sin_port;
}

backroute_t* new_backroute(uv_handle_t* source_handle, const struct sockaddr* source) {
    backroute_t* br = (backroute_t*)malloc(sizeof(backroute_t));
    if (br == NULL)
        return NULL;

    br->source = (struct sockaddr*)malloc(sizeof(struct sockaddr));
    if (br->source == NULL)
        return NULL;

    memcpy(br->source, source, sizeof(struct sockaddr));

    br->source_handle = source_handle;
    return br;
}

void free_backroute(backroute_t* br) {
    if (br) {
        free(br->source);
        free(br);
    }
}