//
// Created by mahdi on 7/28/25.
//
#pragma once

#define LOG(format, name, ...) fprintf(stderr, "[%s_%d] " format, name, worker_number, ##__VA_ARGS__)

// Caller MUST free the result
char* to_hex_string(const char* data, size_t len);
const struct sockaddr_in convert_payload_to_destination(char* payload);
const char* convert_destination_to_payload(const struct sockaddr_in* addr);
const char* generate_unique_id(const struct sockaddr_in* a1, const struct sockaddr_in* a2);
