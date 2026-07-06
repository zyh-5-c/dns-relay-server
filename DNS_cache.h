#pragma once

#include "DNS_struct.h"
#include "DNS_config.h"

#define MAX_CACHE 16

typedef struct nodes {
    uint8_t IP[4];
    char domain[DNS_RR_NAME_MAX_SIZE];
    struct nodes* next;
} lru_cache;

extern lru_cache* head;
extern lru_cache* tail;
extern int cache_size;

void init_cache(void);
int cache_query(uint8_t* ipv4, char* domain_name);
void insert_cache(const uint8_t ipv4[4], char* domain_name);
void delete_node(void);
