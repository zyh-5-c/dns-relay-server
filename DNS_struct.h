#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <WinSock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")
#pragma warning(disable:4996)

#define DNS_RR_NAME_MAX_SIZE 512

#define DNS_TYPE_A 1
#define DNS_TYPE_AAAA 28

#define DNS_CLASS_IN 1

#define DNS_RCODE_OK 0
#define DNS_RCODE_NXDOMAIN 3

/*
 * flags 的各个位直接按 DNS 协议定义保存。
 */
#define DNS_QR_MASK 0x8000
#define DNS_OPCODE_MASK 0x7800
#define DNS_AA_MASK 0x0400
#define DNS_TC_MASK 0x0200
#define DNS_RD_MASK 0x0100
#define DNS_RA_MASK 0x0080
#define DNS_RCODE_MASK 0x000F

typedef struct DNS_header {
    uint16_t id;          /* 请求/响应 ID */
    uint16_t flags;       /* DNS 头部标志位 */
    uint16_t qdcount;     /* Question 数量 */
    uint16_t ancount;     /* Answer 数量 */
    uint16_t nscount;     /* Authority 数量 */
    uint16_t arcount;     /* Additional 数量 */
} Dns_Header;

typedef struct DNS_question {
    char* q_name;
    uint16_t q_type;
    uint16_t q_class;
} Dns_Question;

typedef struct DNS_resource_record {
    char* name;
    uint16_t type;
    uint16_t rr_class;
    uint32_t ttl;
    uint16_t rd_length;

    /*
     * - A 记录：4 字节 IPv4
     * - AAAA 记录：16 字节 IPv6
     */
    uint8_t rdata[16];
} Dns_rr;

typedef struct DNS_mes {
    Dns_Header* header;
    Dns_Question* question;
    Dns_rr* answer;
} Dns_Mes;
