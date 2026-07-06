#pragma once

#include "DNS_struct.h"

void string_to_dnsstruct(Dns_Mes* pmsg, uint8_t* buffer, uint8_t* start);
size_t read_bits(uint8_t** buffer, int bits);
uint8_t* get_dnsheader(Dns_Mes* msg, uint8_t* buffer);
uint8_t* get_dnsquestion(Dns_Mes* msg, uint8_t* buffer, uint8_t* start);
uint8_t* get_dnsanswer(Dns_Mes* msg, uint8_t* buffer, uint8_t* start);
uint8_t* get_domain(uint8_t* buffer, char* name, uint8_t* start);
uint8_t* dnsstruct_to_string(Dns_Mes* pmsg, uint8_t* buffer, uint8_t* ip_addr);
void write_bits(uint8_t** buffer, int bits, int value);
uint8_t* set_dnsheader(Dns_Mes* msg, uint8_t* buffer, uint8_t* ip_addr);
uint8_t* set_dnsquestion(Dns_Mes* msg, uint8_t* buffer);
uint8_t* set_dnsanswer(Dns_Mes* msg, uint8_t* buffer, uint8_t* ip_addr);
uint8_t* set_domain(uint8_t* buffer, char* name);
void free_message(Dns_Mes* msg);
