#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "DNS_config.h"
#include "DNS_convert.h"
#include "DNS_print.h"

/*
 * 这个文件负责 DNS 报文和程序内部结构之间的双向转换。
 *
 * 可以把它理解成两类工作：
 * 1. 把网络上收到的 DNS 字节流拆开，解析成结构体；
 * 2. 把程序已经准备好的结构体，再编码回 DNS 报文。
 *
 * 当前课程设计只保留最核心、最容易理解的部分：
 * - 只解析 1 个 Question；
 * - 只重点保留第 1 个 Answer；
 * - 本地只构造 A 记录响应；
 * - 本地拦截时返回 NXDOMAIN。
 */

static int is_blocked_ip(const uint8_t* ip_addr) {
	/* 0.0.0.0 返回域名不存在 */
	return ip_addr[0] == 0 && ip_addr[1] == 0 && ip_addr[2] == 0 && ip_addr[3] == 0;
}

size_t read_bits(uint8_t** buffer, int bits) {
	/*
	 * 二级指针，从当前指针位置读取固定长度的数据，并自动把指针向后推进。
	 *
	 * 这里做了两件事：
	 * 1. 按 8 / 16 / 32 位读取；
	 * 2. 对 16 / 32 位数据做网络字节序 -> 主机字节序转换。
	 *
	 * 原因是 DNS 报文里的多字节整数都按网络字节序保存，
	 * 直接 memcpy 到本机变量后不能直接使用，必须经过 ntohs / ntohl。
	 */
	if (bits == 8) {
		uint8_t val = 0;
		memcpy(&val, *buffer, 1);
		*buffer += 1;
		return val;
	}

	if (bits == 16) {
		uint16_t val = 0;
		memcpy(&val, *buffer, 2);
		*buffer += 2;
		return ntohs(val);
	}

	if (bits == 32) {
		uint32_t val = 0;
		memcpy(&val, *buffer, 4);
		*buffer += 4;
		return ntohl(val);
	}

	return 0;
}

static void init_message(Dns_Mes* msg) {
	memset(msg, 0, sizeof(*msg));
	msg->header = (Dns_Header*)calloc(1, sizeof(Dns_Header));
}

void string_to_dnsstruct(Dns_Mes* pmsg, uint8_t* buffer, uint8_t* start) {
	/*
	 * 字节流 -> 结构体
	 *
	 * 解析顺序和 DNS 报文格式保持一致：
	 * 1. Header
	 * 2. Question
	 * 3. Answer
	 *
	 * start 始终指向整个报文起始位置，
	 * 后面解析域名压缩指针时需要靠它做相对偏移跳转。
	 */
	init_message(pmsg);

    if (debug_mode >= 2) {
        printf("Decoded DNS message:\n");
    }

    buffer = get_dnsheader(pmsg, buffer);
    if (debug_mode >= 2) {
        print_header(pmsg);
    }

    buffer = get_dnsquestion(pmsg, buffer, start);
    if (debug_mode >= 2) {
        print_question(pmsg);
    }

    buffer = get_dnsanswer(pmsg, buffer, start);
    if (debug_mode >= 2) {
        print_answer(pmsg);
    }
}

uint8_t* get_dnsheader(Dns_Mes* msg, uint8_t* buffer) {
	/*
	 * DNS Header 固定 12 字节，所以按固定顺序直接读取即可。
	 * flags由QR / AA / RD / RA / RCODE 组成
	 */
	msg->header->id = (uint16_t)read_bits(&buffer, 16);
	msg->header->flags = (uint16_t)read_bits(&buffer, 16);
	msg->header->qdcount = (uint16_t)read_bits(&buffer, 16);
	msg->header->ancount = (uint16_t)read_bits(&buffer, 16);
	msg->header->nscount = (uint16_t)read_bits(&buffer, 16);
	msg->header->arcount = (uint16_t)read_bits(&buffer, 16);
	return buffer;
}

uint8_t* get_dnsquestion(Dns_Mes* msg, uint8_t* buffer, uint8_t* start) {
	/*
	 * - qdcount 为 0：说明没有问题段，直接返回；
	 *
	 * 一个 Question 的格式是：
	 *   QNAME + QTYPE + QCLASS
	 */
	if (msg->header->qdcount == 0) {
		return buffer;
	}

	char name[DNS_RR_NAME_MAX_SIZE] = { 0 };

	/* 先把 QNAME 从 DNS 报文格式还原成普通域名字符串。 */
	buffer = get_domain(buffer, name, start);

	Dns_Question* node = (Dns_Question*)calloc(1, sizeof(Dns_Question));
	if (node == NULL) {
		return buffer;
	}

	node->q_name = (char*)malloc(strlen(name) + 1);
	if (node->q_name == NULL) {
		free(node);
		return buffer;
	}

	strcpy(node->q_name, name);
	node->q_type = (uint16_t)read_bits(&buffer, 16);
	node->q_class = (uint16_t)read_bits(&buffer, 16);
	msg->question = node;

	return buffer;
}

uint8_t* get_dnsanswer(Dns_Mes* msg, uint8_t* buffer, uint8_t* start) {

	for (int i = 0; i < msg->header->ancount; ++i) {//Answer 数量
		char name[DNS_RR_NAME_MAX_SIZE] = { 0 };
		buffer = get_domain(buffer, name, start);

		uint16_t type = (uint16_t)read_bits(&buffer, 16);
		uint16_t rr_class = (uint16_t)read_bits(&buffer, 16);
		uint32_t ttl = (uint32_t)read_bits(&buffer, 32);
		uint16_t rd_length = (uint16_t)read_bits(&buffer, 16);

		uint8_t rdata[16] = { 0 };
		if (type == DNS_TYPE_A) {
			for (int j = 0; j < 4 && j < rd_length; j++) {
				rdata[j] = (uint8_t)read_bits(&buffer, 8);
			}
			if (rd_length > 4) {
				buffer += (rd_length - 4);
			}
		}
		else if (type == DNS_TYPE_AAAA) {
			for (int j = 0; j < 16 && j < rd_length; j++) {
				rdata[j] = (uint8_t)read_bits(&buffer, 8);
			}
			if (rd_length > 16) {
				buffer += (rd_length - 16);
			}
		}
		else {
			buffer += rd_length;
		}

		if (msg->answer == NULL && (type == DNS_TYPE_A || type == DNS_TYPE_AAAA)) {
			Dns_rr* node = (Dns_rr*)calloc(1, sizeof(Dns_rr));
			if (node == NULL) {
				return buffer;
			}

			node->name = (char*)malloc(strlen(name) + 1);
			if (node->name == NULL) {
				free(node);
				return buffer;
			}

			strcpy(node->name, name);
			node->type = type;
			node->rr_class = rr_class;
			node->ttl = ttl;
			node->rd_length = rd_length;
			memcpy(node->rdata, rdata, sizeof(node->rdata));

			msg->answer = node;
		}
	}

	return buffer;
}

uint8_t* get_domain(uint8_t* buffer, char* name, uint8_t* start) {
	/*
	 * DNS 报文里的域名不是普通字符串，而是 label 编码。
	 * 例如：
	 *   3 www 5 baidu 3 com 0
	 * 表示：
	 *   www.baidu.com
	 *
	 * 另外 DNS 还支持域名压缩指针，
	 * 所以这个函数还要处理 0xC0 开头的偏移跳转。
	 */
	uint8_t* ptr = buffer;
	int i = 0;

	while (*ptr != 0) {
		if ((*ptr & 0xC0) == 0xC0) {//0xC0是一个字节
			/*
			 * 如果当前两个字节前两位是 11，
			 * 说明这不是普通 label 长度，而是一个“压缩指针”。
			 *
			 * 这个指针后面的 14 位表示：
			 * 要跳到整个报文中的哪个偏移位置继续解析域名。
			 */
			uint16_t offset = (uint16_t)((ptr[0] & 0x3F) << 8) | ptr[1];
			get_domain(start + offset, name + i, start);
			return ptr + 2;
		}

		int len = *ptr++;
		if (i != 0) {
			name[i++] = '.';
		}

		for (int j = 0; j < len; j++) {
			name[i++] = (char)(*ptr++);
		}
	}
	//循环结束
	name[i] = '\0';
	return ptr + 1;//因为 ptr 当前指向结尾的 00，所以返回 ptr + 1，也就是域名字段后面的下一个位置
}

void write_bits(uint8_t** buffer, int bits, int value) {
	/*
	 * 这是 read_bits 的反方向版本：
	 * 把整数写回字节流，并向后推进写指针。
	 *
	 * 对 16 / 32 位值同样需要先转成网络字节序，
	 * 否则构造出来的 DNS 报文格式会错误。
	 */
	if (bits == 8) {
		**buffer = (uint8_t)value;
		(*buffer)++;
	}
	else if (bits == 16) {
		uint16_t val = htons((uint16_t)value);
		memcpy(*buffer, &val, 2);
		*buffer += 2;
	}
	else if (bits == 32) {
		uint32_t val = htonl((uint32_t)value);
		memcpy(*buffer, &val, 4);
		*buffer += 4;
	}
}
//dnsstruct_to_string() 只在本地能够回答时使用
uint8_t* dnsstruct_to_string(Dns_Mes* pmsg, uint8_t* buffer, uint8_t* ip_addr) {
	/*
	 * 这是“结构体 -> 字节流”的总入口。
	 * 当前本地响应编码顺序是：
	 *   Header -> Question -> Answer
	 */
	buffer = set_dnsheader(pmsg, buffer, ip_addr);
	buffer = set_dnsquestion(pmsg, buffer);
	buffer = set_dnsanswer(pmsg, buffer, ip_addr);
	return buffer;
}

uint8_t* set_dnsheader(Dns_Mes* msg, uint8_t* buffer, uint8_t* ip_addr) {
	Dns_Header* header = msg->header;
	int blocked = is_blocked_ip(ip_addr);
	uint16_t flags = header->flags;

	/*
	 * 本地回包时，不需要凭空新建一个 Header，
	 * 最简单的做法就是在原请求 Header 的基础上改造成响应报。
	 *
	 * 这里主要做四件事：
	 * 1. 把报文标记成响应报；
	 * 2. 标记成本地权威回答；
	 * 3. 标记递归可用；
	 * 4. 根据是否拦截，写入 OK 或 NXDOMAIN。
	 *
	 * 如果命中的是 0.0.0.0：
	 * - RCODE 设为 NXDOMAIN；
	 * - ancount 设为 0；
	 * - 后面不再写 Answer 段。
	 */
	flags &= ~(DNS_QR_MASK | DNS_AA_MASK | DNS_TC_MASK | DNS_RA_MASK | DNS_RCODE_MASK);
	flags |= DNS_QR_MASK;//QR = 1，这是 DNS 响应，不是查询请求
	flags |= DNS_AA_MASK;//AA = 1，权威回答
	flags |= DNS_RA_MASK;//支持递归/中继查询
	flags |= blocked ? DNS_RCODE_NXDOMAIN : DNS_RCODE_OK;

	header->flags = flags;
	header->qdcount = (msg->question != NULL) ? 1 : 0;
	header->ancount = blocked ? 0 : 1;
	header->nscount = 0;
	header->arcount = 0;

	write_bits(&buffer, 16, header->id);
	write_bits(&buffer, 16, header->flags);
	write_bits(&buffer, 16, header->qdcount);
	write_bits(&buffer, 16, header->ancount);
	write_bits(&buffer, 16, header->nscount);
	write_bits(&buffer, 16, header->arcount);
	return buffer;
}

uint8_t* set_dnsquestion(Dns_Mes* msg, uint8_t* buffer) {
	/*
	 * 本地响应里通常要把原问题段带回去，
	 * 这样客户端能知道“这是对哪个查询的回答”。
	 */
	if (msg->question != NULL) {
		buffer = set_domain(buffer, msg->question->q_name);
		write_bits(&buffer, 16, msg->question->q_type);
		write_bits(&buffer, 16, msg->question->q_class);
	}

	return buffer;
}

uint8_t* set_dnsanswer(Dns_Mes* msg, uint8_t* buffer, uint8_t* ip_addr) {
	/*
	 * 这里只构造最简单的本地 A 记录响应。
	 *
	 * 如果是拦截域名（0.0.0.0），
	 * Header 已经写成 NXDOMAIN，这里就不再生成 Answer。
	 */
	if (msg->question == NULL || is_blocked_ip(ip_addr)) {
		return buffer;
	}

	/*
	 * 一个本地 A 记录 Answer 的字段顺序是：
	 *   NAME + TYPE + CLASS + TTL + RDLENGTH + RDATA
	 *
	 * 这里：
	 * - TYPE 固定写 A
	 * - CLASS 固定写 IN
	 * - RDLENGTH 固定为 4
	 * - RDATA 就是 4 字节 IPv4 地址
	 */
	buffer = set_domain(buffer, msg->question->q_name);
	write_bits(&buffer, 16, DNS_TYPE_A);
	write_bits(&buffer, 16, DNS_CLASS_IN);
	write_bits(&buffer, 32, 4);
	write_bits(&buffer, 16, 4);

	for (int i = 0; i < 4; i++) {
		*buffer++ = ip_addr[i];
	}

	return buffer;
}

uint8_t* set_domain(uint8_t* buffer, char* name) {
	char label[DNS_RR_NAME_MAX_SIZE] = { 0 };
	int label_len = 0;

	/*
	 * 把普通字符串域名重新编码成 DNS 的 label 格式。
	 *
	 * 例如：
	 *   www.example.com
	 * 会写成：
	 *   3 www 7 example 3 com 0
	 */
	for (const char* ptr = name;; ptr++) {
		if (*ptr == '.' || *ptr == '\0') {
			*buffer++ = (uint8_t)label_len;
			memcpy(buffer, label, label_len);
			buffer += label_len;//buffer 指针向后移动 label_len 个字节
			memset(label, 0, sizeof(label));
			label_len = 0;

			if (*ptr == '\0') {
				*buffer++ = 0;
				break;
			}
		}
		else {
			label[label_len++] = *ptr;
		}
	}

	return buffer;
}

void free_message(Dns_Mes* msg) {
	/*
	 * 解析过程中为 Header、Question、Answer 动态分配了内存，
	 * 这里统一释放，避免内存泄漏。
	 */
	if (msg == NULL) {
		return;
	}

	free(msg->header);
	msg->header = NULL;

	Dns_Question* question = msg->question;
	if (question != NULL) {
		free(question->q_name);
		free(question);
	}
	msg->question = NULL;

	Dns_rr* answer = msg->answer;
	if (answer != NULL) {
		free(answer->name);
		free(answer);
	}
	msg->answer = NULL;
}
