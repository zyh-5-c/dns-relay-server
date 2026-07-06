#pragma once

#include "DNS_struct.h"

extern char IPAddr[DNS_RR_NAME_MAX_SIZE];
extern char domain[DNS_RR_NAME_MAX_SIZE];
extern char* host_path;

extern int debug_mode;

void init(int argc, char* argv[]);
void get_config(int argc, char* argv[]);
void print_help_info(void);
void read_host(void);
void get_host_info(FILE* ptr);
