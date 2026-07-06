#include "DNS_Hash.h"

#define GREEN "\x1B[32m"
#define RESET "\x1B[0m"

/*
 * 本地 hosts 表使用“哈希表 + 链表冲突处理”实现。
 *
 * 这样做的原因很直接：
 * 1. 题目要求的是完整域名精确匹配，不是通配符匹配；
 * 2. 哈希表查找逻辑比 Trie 更短，更容易讲清楚；
 * 3. 冲突时用链表连接即可，足够满足本次课程设计规模。
 */

host_entry* host_table[HASH_BUCKET_COUNT];

static unsigned int hash_domain(const char* domain_name) {
    unsigned int hash = 5381U;

    while (*domain_name != '\0') {
        unsigned char ch = (unsigned char)*domain_name++;
        if (ch >= 'A' && ch <= 'Z') {
            ch = (unsigned char)(ch + ('a' - 'A'));
        }
        hash = ((hash << 5) + hash) + ch;
    }

    return hash % HASH_BUCKET_COUNT;
}

void parse_ipv4_string(uint8_t* ip, char* address) {
    /*
     * 把形如 1.2.3.4 的点分十进制字符串，
     * 解析成 4 字节 IPv4 地址。
     */
    int len = (int)strlen(address);
    int number = 0;
    int count = 0;

    for (int i = 0; i < len; i++) {
        if (address[i] != '.') {
            number = number * 10 + (address[i] - '0');
        } else if (count < 4) {
            ip[count++] = (uint8_t)number;
            number = 0;
        }
    }

    if (count < 4) {
        ip[count] = (uint8_t)number;
    }
}

void insert_host_entry(uint8_t* ip, char* domain_name) {
    unsigned int bucket = hash_domain(domain_name);
    host_entry* current = host_table[bucket];

    /*
     * 如果同一个域名在文件里重复出现后读到的覆盖前面的
     */
    while (current != NULL) {
        if (_stricmp(current->domain, domain_name) == 0) {
            memcpy(current->IP, ip, sizeof(current->IP));
            return;
        }
        current = current->next;
    }

    host_entry* node = (host_entry*)malloc(sizeof(host_entry));
    if (node == NULL) {
        return;
    }

    memset(node, 0, sizeof(host_entry));
    strncpy(node->domain, domain_name, DNS_RR_NAME_MAX_SIZE - 1);
    memcpy(node->IP, ip, sizeof(node->IP));

    /*
     * 头插法把新结点挂到对应桶的链表前端。
     */
    node->next = host_table[bucket];
    host_table[bucket] = node;
}

int lookup_host_entry(char* domain_name, uint8_t* ip_addr) {
    unsigned int bucket = hash_domain(domain_name);
    host_entry* current = host_table[bucket];

    while (current != NULL) {
        if (_stricmp(current->domain, domain_name) == 0) {
            memcpy(ip_addr, current->IP, sizeof(current->IP));

            /*
             * 本地表命中后，顺便把结果写入运行时缓存。
             * 这样后续重复查询可以直接从缓存返回。
             */
            insert_cache(current->IP, domain_name);

            return 1;
        }
        current = current->next;
    }

    return 0;
}
