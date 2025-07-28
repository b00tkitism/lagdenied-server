//
// Created by mahdi on 7/28/25.
//
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

char* to_hex_string(const char* data, size_t len) {
	const char hex_digits[] = "0123456789abcdef";
	char* hex_str = malloc(len * 2 + 1);
	if (!hex_str) return NULL;

	for (size_t i = 0; i < len; ++i) {
		hex_str[i * 2] = hex_digits[(data[i] >> 4) & 0xF];
		hex_str[i * 2 + 1] = hex_digits[data[i] & 0xF];
	}
	hex_str[len * 2] = '\0';
	return hex_str;
}

const struct sockaddr_in convert_payload_to_destination(char* payload) {
	uint32_t destination_ip_address_decimal = 0;
	uint16_t destination_port = 0;

	memcpy(&destination_ip_address_decimal, payload, sizeof(uint32_t));
	memcpy(&destination_port, payload + sizeof(uint32_t), sizeof(uint16_t));

	struct sockaddr_in server_addr = { 0 };
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = destination_port;
	server_addr.sin_addr.s_addr = destination_ip_address_decimal;

	return server_addr;
}

const char* convert_destination_to_payload(const struct sockaddr_in* addr) {
	char* out_payload = malloc(6);
	if (!out_payload) return NULL;

	memcpy(out_payload, &addr->sin_addr.s_addr, 4);         // 4 bytes of IP
	memcpy(out_payload + 4, &addr->sin_port, 2);            // 2 bytes of port

	return out_payload;
}
