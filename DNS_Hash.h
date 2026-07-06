#pragma once

#include "DNS_cache.h"

#define HASH_BUCKET_COUNT 2048

typedef struct host_entry {
    char domain[DNS_RR_NAME_MAX_SIZE];
    uint8_t IP[4];
    struct host_entry* next;
} host_entry;

extern host_entry* host_table[HASH_BUCKET_COUNT];

void parse_ipv4_string(uint8_t* ip, char* address);
void insert_host_entry(uint8_t* ip, char* domain_name);
int lookup_host_entry(char* domain_name, uint8_t* ip_addr);
