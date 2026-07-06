#pragma once

#include "DNS_struct.h"

extern unsigned int current_debug_query_serial;

void print_dstring(char* pstring, unsigned int length);
void print_debug_event(const char* event_text);
void print_debug_query_summary(unsigned int serial_number, const char* domain_name);
void print_header(Dns_Mes* msg);
void print_question(Dns_Mes* msg);
void print_answer(Dns_Mes* msg);
