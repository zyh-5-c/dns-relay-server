#pragma once

#include "DNS_config.h"
#include "DNS_Hash.h"
#include "ResetID.h"

#define DNS_PORT 53
#define BUFFER_SIZE 1500

extern SOCKET dnsSocket;
extern struct sockaddr_in clientAddress;
extern struct sockaddr_in serverAddress;

extern char* dnsServerAddress;
extern int pending_requests;

void initializeSocket(void);
void closeSocketServer(void);
void run_dns_server(void);
