#include "DNS_config.h"
#include "DNS_server.h"
#include "DNS_Hash.h"
#include "ResetID.h"
#include "DNS_cache.h"

/*
 * 这个文件负责程序启动阶段的所有准备工作。
 *
 * 整体流程：
 * 1. 读取命令行参数，确定是否开启调试，以及远程 DNS 的地址；
 * 2. 初始化网络环境；
 * 3. 初始化 ID 映射表和运行时缓存；
 * 4. 读取 dnsrelay.txt，把本地域名表装入哈希表。
 *
 */

char IPAddr[DNS_RR_NAME_MAX_SIZE];
char domain[DNS_RR_NAME_MAX_SIZE];
char* host_path = "dnsrelay.txt";
int debug_mode = 0;

static int file_exists(const char* path) {
    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        return 0;
    }

    fclose(fp);
    return 1;
}

static FILE* open_host_file(void) {
    /*
     * 检查dnsrelay.txt是否存在
     */
    if (file_exists(host_path)) {
        return fopen(host_path, "r");
    }
    return NULL;
}

void init(int argc, char* argv[]) {
    /*
     * 挂起请求计数表示：
     * 当前已经转发给远程 DNS、但还没收到回应的请求数量。
     */
    pending_requests = 0;

    get_config(argc, argv);
    initializeSocket();
    init_ID_list();
    init_cache();
    read_host();
}

void get_config(int argc, char* argv[]) {
    int positional_count = 0;

    for (int index = 1; index < argc; index++) {
        if (strcmp(argv[index], "-d") == 0) {
            debug_mode = 1;
        } else if (strcmp(argv[index], "-dd") == 0) {
            debug_mode = 2;
        } else if ((strcmp(argv[index], "-h") == 0) || (strcmp(argv[index], "--help") == 0)) {
            print_help_info();
            exit(0);
        } else if (strcmp(argv[index], "-s") == 0 && index + 1 < argc) {
            dnsServerAddress = argv[++index];
        } else if (strcmp(argv[index], "-s") == 0) {
            fprintf(stderr, "Option -s requires a DNS server address.\n");
            print_help_info();
            exit(1);
        } else if (argv[index][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[index]);
            print_help_info();
            exit(1);
        } else if (positional_count == 0) {
            dnsServerAddress = argv[index];
            positional_count++;
        } else if (positional_count == 1) {
            host_path = argv[index];
            positional_count++;
        } else {
            fprintf(stderr, "Too many positional arguments.\n");
            print_help_info();
            exit(1);
        }
    }
}

void print_help_info(void) {
    printf("Usage:\n");
    printf("  dnsrelay [-d | -dd] [dns-server-ipaddr] [filename]\n");
    printf("\n");
    printf("Examples:\n");
    printf("  dnsrelay\n");
    printf("  dnsrelay -d 192.168.0.1 c:\\dns-table.txt\n");
    printf("  dnsrelay -dd 10.3.9.5 dnsrelay.txt\n");
    printf("\n");
    printf("Debug levels:\n");
    printf("  -d   Print timestamp, serial number, and queried domain only.\n");
    printf("  -dd  Print detailed DNS relay diagnostics.\n");
    printf("\n");
    printf("Defaults:\n");
    printf("  DNS server: 10.3.9.5\n");
    printf("  hosts file: dnsrelay.txt\n");
    printf("\n");
    printf("Compatibility:\n");
    printf("  -s [server_address] is still accepted.\n");
}

void read_host(void) {
    /*
     * dnsrelay.txt 是本地规则表。
     * 三种行为：
     * 1. 普通 IP：直接本地应答；
     * 2. 0.0.0.0：返回域名不存在；
     * 3. 查不到：转发给远程 DNS。
     */
    FILE* host = open_host_file();
    if (host == NULL) {
        printf("Error! Can not open hosts file: %s\n\n", host_path);
        exit(1);
    }

    get_host_info(host);
    fclose(host);
}

void get_host_info(FILE* ptr) {
    int num = 0;

    /*
     * dnsrelay.txt 每行格式固定为：
     *   IP地址 域名
     * 例如：
     *   1.2.3.4 www.example.com
     */
    while (fscanf(ptr, "%511s %511s", IPAddr, domain) == 2) {
        uint8_t this_ip[4] = { 0 };
        num++;

        parse_ipv4_string(this_ip, IPAddr);
        insert_host_entry(this_ip, domain);
    }

    if (debug_mode >= 2) {
        printf("%d domain name address records have been loaded.\n\n", num);
    }
}
