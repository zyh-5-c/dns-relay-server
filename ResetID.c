#include "ResetID.h"

/*
 * 这个模块解决“多客户端并发时，远程 DNS 响应如何正确回到原客户端”的问题。
 *
 * 核心思路：
 * 1. 客户端请求到来时，先把原始 ID 保存下来；
 * 2. 分配一个中继内部使用的新 ID 发给远程 DNS；
 * 3. 响应回来后，再根据新 ID 找回原始 ID 和客户端地址。
 *
 * 因为 UDP 可能丢包或迟到，所以每条映射都带过期时间。
 */

ClientSession ID_list[MAX_ID_SIZE];

uint16_t reset_id(uint16_t user_id, struct sockaddr_in client_address) {
    time_t current_time = time(NULL);

    /*
     * 找到一个已经空闲或已经过期的位置，
     * 用它保存这次新的转发请求。
     */
    for (uint16_t i = 0; i < MAX_ID_SIZE; i++) {
        if (ID_list[i].expire_time <= current_time) {
            ID_list[i].user_id = user_id;
            ID_list[i].client_address = client_address;
            ID_list[i].expire_time = current_time + ID_EXPIRE_TIME;
            return i;
        }
    }

    return UINT16_MAX;
}

void init_ID_list(void) {
    memset(ID_list, 0, sizeof(ID_list));
}
