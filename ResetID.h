#pragma once

#include "DNS_cache.h"

#define MAX_ID_SIZE 200
#define ID_EXPIRE_TIME 4

typedef struct {
    uint16_t user_id;
    time_t expire_time;
    struct sockaddr_in client_address;
} ClientSession;

extern ClientSession ID_list[MAX_ID_SIZE];

uint16_t reset_id(uint16_t user_id, struct sockaddr_in client_address);
void init_ID_list(void);
