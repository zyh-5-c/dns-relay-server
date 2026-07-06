#include "DNS_cache.h"

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define RESET "\x1B[0m"

/*
 * 这里实现一个非常基础的 LRU 风格缓存。
 *
 * 设计目标不是做复杂高性能缓存，而是：
 * 1. 让重复查询可以直接命中；
 * 2. 让课程设计里“DNS 缓存”这部分有明确、可读的实现；
 * 3. 保持结构足够简单，便于讲解。
 */

lru_cache* head = NULL;
lru_cache* tail = NULL;
int cache_size = 0;

void init_cache(void) {
    /*
     * 使用一个不存业务数据的头结点，
     * 可以简化插入、移动和删除逻辑。
     */
    head = (lru_cache*)malloc(sizeof(lru_cache));
    if (head == NULL) {
        fprintf(stderr, "Failed to initialize cache.\n");
        exit(1);
    }

    memset(head, 0, sizeof(lru_cache));
    tail = head;
    cache_size = 0;
}

int cache_query(uint8_t* ipv4, char* domain_name) {
    lru_cache* prev = head;

    while (prev != NULL && prev->next != NULL) {
        if (strcmp(prev->next->domain, domain_name) == 0) {
            lru_cache* hit = prev->next;
            memcpy(ipv4, hit->IP, sizeof(hit->IP));

            /*
             * 命中后把该结点移动到链表前面，
             */
            if (prev != head) {
                prev->next = hit->next;
                hit->next = head->next;
                head->next = hit;
                if (tail == hit) {
                    tail = prev;
                }
            }

            return 1;
        }
        prev = prev->next;
    }

    return 0;
}

void insert_cache(const uint8_t ipv4[4], char* domain_name) {
    uint8_t cached_ip[4];

    /*
     * 如果缓存里已经有这个域名，直接更新并把它维持在链表前部
     */
    if (cache_query(cached_ip, domain_name)) {
        memcpy(head->next->IP, ipv4, sizeof(head->next->IP));
        return;
    }

    if (cache_size >= MAX_CACHE) {
        delete_node();
    }

    lru_cache* node = (lru_cache*)malloc(sizeof(lru_cache));
    if (node == NULL) {
        fprintf(stderr, "Failed to allocate cache node.\n");
        return;
    }

    memset(node, 0, sizeof(lru_cache));
    memcpy(node->IP, ipv4, sizeof(node->IP));
    strncpy(node->domain, domain_name, DNS_RR_NAME_MAX_SIZE - 1);

    node->next = head->next;
    head->next = node;

    if (tail == head) {
        tail = node;
    }

    cache_size++;
}

void delete_node(void) {
    if (head == NULL || head->next == NULL) {
        return;
    }

    /*
     * 尾部表示“最近最少使用”。
     * 删除时只需要找到尾结点的前驱，再把尾结点释放掉。
     */
    lru_cache* prev = head;
    while (prev->next != NULL && prev->next->next != NULL) {
        prev = prev->next;
    }

    free(prev->next);
    prev->next = NULL;
    tail = prev;

    if (cache_size > 0) {
        cache_size--;
    }
}
