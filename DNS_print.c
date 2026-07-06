#include "DNS_print.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>

#define YELLOW "\x1B[33m"
#define RESET "\x1B[0m"

unsigned int current_debug_query_serial = 0;

void print_dstring(char* pstring, unsigned int length) {
    (void)pstring;
    (void)length;
}

void print_debug_event(const char* event_text) {
    if (current_debug_query_serial > 0) {
        printf("[Query #%u] %s\n\n", current_debug_query_serial, event_text);
    } else {
        printf("%s\n\n", event_text);
    }
}

void print_debug_query_summary(unsigned int serial_number, const char* domain_name) {
    struct _timeb now;
    struct tm local_now;

    _ftime_s(&now);
    localtime_s(&local_now, &now.time);

    printf("[Query #%u] %02d:%02d:%02d.%03hu %s\n",
        serial_number,
        local_now.tm_hour,
        local_now.tm_min,
        local_now.tm_sec,
        now.millitm,
        domain_name != NULL ? domain_name : "<unknown>");
}

void print_header(Dns_Mes* msg) {
    uint16_t flags = msg->header->flags;
    int qr = (flags & DNS_QR_MASK) >> 15;
    int opcode = (flags & DNS_OPCODE_MASK) >> 11;
    int aa = (flags & DNS_AA_MASK) >> 10;
    int tc = (flags & DNS_TC_MASK) >> 9;
    int rd = (flags & DNS_RD_MASK) >> 8;
    int ra = (flags & DNS_RA_MASK) >> 7;
    int rcode = flags & DNS_RCODE_MASK;

    if (current_debug_query_serial > 0) {
        printf(YELLOW "----------------------- Query #%u Header ----------------------\n" RESET, current_debug_query_serial);
    } else {
        printf(YELLOW "----------------------------header----------------------------\n" RESET);
    }
    printf("ID = %d\n", msg->header->id);
    printf("flags = 0x%04X\n", flags);
    printf("qr = %d, opcode = %d\n", qr, opcode);
    printf("aa = %d, tc = %d, rd = %d, ra = %d\n", aa, tc, rd, ra);
    printf("rcode = %d\n", rcode);
    printf("qdCount = %d\n", msg->header->qdcount);
    printf("anCount = %d\n", msg->header->ancount);
    printf("nsCount = %d\n", msg->header->nscount);
    printf("arCount = %d\n", msg->header->arcount);
}

void print_question(Dns_Mes* msg) {
    if (msg->question == NULL) {
        return;
    }

    if (current_debug_query_serial > 0) {
        printf(YELLOW "---------------------- Query #%u Question ---------------------\n" RESET, current_debug_query_serial);
    } else {
        printf(YELLOW "----------------------------question--------------------------\n" RESET);
    }
    printf("domain: %s\n", msg->question->q_name);
    printf("query type: %d\n", msg->question->q_type);
    printf("query class: %d\n", msg->question->q_class);
}

void print_answer(Dns_Mes* msg) {
    if (msg->header->ancount == 0 || msg->answer == NULL) {
        printf("\n");
        return;
    }

    if (current_debug_query_serial > 0) {
        printf(YELLOW "----------------------- Query #%u Answer ----------------------\n" RESET, current_debug_query_serial);
    } else {
        printf(YELLOW "----------------------------answer----------------------------\n" RESET);
    }
    printf("domain: %s\n", msg->answer->name);
    printf("answer type: %d\n", msg->answer->type);
    printf("resource record class: %d\n", msg->answer->rr_class);
    printf("time to live: %d\n", msg->answer->ttl);
    printf("record length: %d\n", msg->answer->rd_length);

    if (msg->answer->type == DNS_TYPE_A) {
        printf("A Record: %d.%d.%d.%d",
            msg->answer->rdata[0],
            msg->answer->rdata[1],
            msg->answer->rdata[2],
            msg->answer->rdata[3]);
    }
    printf("\n\n");
}
