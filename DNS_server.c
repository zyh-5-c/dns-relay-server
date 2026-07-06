#include "DNS_server.h"
#include "DNS_convert.h"
#include "DNS_print.h"

#define RED "\x1B[31m"
#define BLUE "\x1B[34m"
#define RESET "\x1B[0m"

SOCKET dnsSocket = INVALID_SOCKET;
struct sockaddr_in clientAddress;
struct sockaddr_in serverAddress;
char* dnsServerAddress = "10.3.9.5";
int pending_requests = 0;
static unsigned int client_query_serial = 0;
static unsigned int relay_query_serial[MAX_ID_SIZE] = { 0 };

static int is_a_query(const Dns_Mes* msg) {
	return msg != NULL && msg->question != NULL && msg->question->q_type == DNS_TYPE_A;
}

static int is_blocked_ipv4(const uint8_t* ip_addr) {
	return ip_addr != NULL
		&& ip_addr[0] == 0
		&& ip_addr[1] == 0
		&& ip_addr[2] == 0
		&& ip_addr[3] == 0;
}

static void cleanup_message(Dns_Mes* msg) {
	if (msg != NULL) {
		free_message(msg);
	}
}

static int is_from_remote_dns(const struct sockaddr_in* addr) {
	return addr != NULL
		&& addr->sin_family == serverAddress.sin_family
		&& addr->sin_addr.s_addr == serverAddress.sin_addr.s_addr
		&& addr->sin_port == serverAddress.sin_port;
}

static void handle_remote_response(uint8_t* buffer, int msg_size) {
	Dns_Mes msg = { 0 };
	uint16_t relay_id = 0;

	if (msg_size >= (int)sizeof(uint16_t)) {
		uint16_t network_id = 0;
		memcpy(&network_id, buffer, sizeof(network_id));
		relay_id = ntohs(network_id);
	}

	current_debug_query_serial = (relay_id < MAX_ID_SIZE) ? relay_query_serial[relay_id] : 0;

	if (debug_mode >= 2) {
		if (current_debug_query_serial > 0) {
			printf(BLUE "[Query #%u] Received one message from remote server.\n\n" RESET, current_debug_query_serial);
		}
		else {
			printf(BLUE "Received one message from remote server.\n\n" RESET);
		}
	}

	string_to_dnsstruct(&msg, buffer, buffer);
	relay_id = msg.header->id;

	if (relay_id >= MAX_ID_SIZE || ID_list[relay_id].expire_time == 0 || ID_list[relay_id].expire_time < time(NULL)) {
		//判断：远程 DNS 回来的这个响应还能不能用
		cleanup_message(&msg);
		return;
	}

	uint16_t old_id = htons(ID_list[relay_id].user_id);
	memcpy(buffer, &old_id, sizeof(old_id));

	struct sockaddr_in original_client = ID_list[relay_id].client_address;
	ID_list[relay_id].expire_time = 0;
	relay_query_serial[relay_id] = 0;

	if (pending_requests > 0) {
		pending_requests--;
	}

	if (msg.question != NULL && msg.answer != NULL && msg.answer->type == DNS_TYPE_A) {
		insert_cache(msg.answer->rdata, msg.question->q_name, msg.answer->ttl);
	}

	sendto(dnsSocket, (const char*)buffer, msg_size, 0, (struct sockaddr*)&original_client, sizeof(original_client));
	if (debug_mode >= 2) {
		print_debug_event("Returned remote DNS response to client.");
	}

	cleanup_message(&msg);
}

static void handle_client_request(uint8_t* buffer, int msg_size, const struct sockaddr_in* request_client, int request_addr_len) {
	uint8_t buffer_new[BUFFER_SIZE];
	Dns_Mes msg = { 0 };
	uint8_t ip_addr[4] = { 0 };

	client_query_serial++;
	current_debug_query_serial = client_query_serial;

	if (debug_mode >= 2) {
		printf(BLUE "[Query #%u] Received one message from client.\n\n" RESET, current_debug_query_serial);
	}

	string_to_dnsstruct(&msg, buffer, buffer);

	if (msg.question != NULL && debug_mode >= 1) {
		print_debug_query_summary(current_debug_query_serial, msg.question->q_name);
	}

	int is_found = 0;
	int should_answer_locally = is_a_query(&msg);
	int blocked_by_local_rule = 0;

	if (msg.question != NULL) {
		if (should_answer_locally) {
			is_found = cache_query(ip_addr, msg.question->q_name);
			//cache_query 会把缓存里的 IP 复制到 ip_addr
			if (is_found != 0) {
				if (debug_mode >= 2) {
					print_debug_event("Cache hit.");
				}
			}
			else {
				if (debug_mode >= 2) {
					print_debug_event("Cache miss.");
				}
				is_found = lookup_host_entry(msg.question->q_name, ip_addr);
				//lookup_host_entry 会把 IP 复制到 ip_addr
				if (is_found != 0 && debug_mode >= 2) {
					print_debug_event("Local table hit.");
				}
			}
		}
		else {
			uint8_t local_ip[4] = { 0 };
			if (lookup_host_entry(msg.question->q_name, local_ip) != 0 && is_blocked_ipv4(local_ip)) {
				memcpy(ip_addr, local_ip, sizeof(ip_addr));
				is_found = 1;
				blocked_by_local_rule = 1;
				if (debug_mode >= 2) {
					print_debug_event("Local block rule hit.");
				}
			}
		}
	}

	if ((!should_answer_locally && !blocked_by_local_rule) || is_found == 0) {
		if (!should_answer_locally && debug_mode >= 2) {
			print_debug_event("Non-A query. Forwarding to remote DNS server.");
		}

		uint16_t new_id = reset_id(msg.header->id, *request_client);//分配中继id
		if (new_id == UINT16_MAX) {
			printf("ID list is full.\n\n");
			cleanup_message(&msg);
			return;
		}

		uint16_t network_id = htons(new_id);
		memcpy(buffer, &network_id, sizeof(network_id));

		if (sendto(dnsSocket, (const char*)buffer, msg_size, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
			ID_list[new_id].expire_time = 0;
			relay_query_serial[new_id] = 0;
		}
		else {
			pending_requests++;
			relay_query_serial[new_id] = current_debug_query_serial;
			if (debug_mode >= 2) {
				printf(RED "[Query #%u] Forwarded request to remote DNS server.\n\n" RESET, current_debug_query_serial);
			}
		}

		cleanup_message(&msg);
		return;
	}
	//命中的情况，构造 DNS 响应并发回客户端
	uint8_t* end = dnsstruct_to_string(&msg, buffer_new, ip_addr);
	int len = (int)(end - buffer_new);
	sendto(dnsSocket, (const char*)buffer_new, len, 0, (const struct sockaddr*)request_client, request_addr_len);
	if (debug_mode >= 2) {
		print_debug_event("Answered locally.");
	}

	cleanup_message(&msg);
}

void initializeSocket(void) {
	WORD wVersionRequested = MAKEWORD(2, 2);
	WSADATA wsaData;

	if (WSAStartup(wVersionRequested, &wsaData) != 0) {//初始化 Windows 的 Winsock 网络库
		fprintf(stderr, "WSAStartup failed: %d\n", WSAGetLastError());
		exit(1);
	}

	dnsSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (dnsSocket == INVALID_SOCKET) {
		fprintf(stderr, "Error opening DNS socket: %d\n", WSAGetLastError());
		WSACleanup();
		exit(1);
	}

	memset(&clientAddress, 0, sizeof(clientAddress));
	clientAddress.sin_family = AF_INET;
	clientAddress.sin_addr.s_addr = INADDR_ANY;
	clientAddress.sin_port = htons(DNS_PORT);

	memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = inet_addr(dnsServerAddress);//inet_addr 是一个把 IPv4 字符串地址 转成 网络字节序整数 的函数
	serverAddress.sin_port = htons(DNS_PORT);

	const int reuse_addr = 1;
	if (setsockopt(dnsSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse_addr, sizeof(reuse_addr)) == SOCKET_ERROR) {
		fprintf(stderr, "setsockopt(SO_REUSEADDR) failed: %d\n", WSAGetLastError());
		closeSocketServer();
		exit(1);
	}

	if (bind(dnsSocket, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) == SOCKET_ERROR) {
		fprintf(stderr, "Bind failed with error: %d\n", WSAGetLastError());
		closeSocketServer();
		exit(1);
	}

	printf("DNS server: %s\n", dnsServerAddress);
	printf("Listening on port %d\n", DNS_PORT);
}

void closeSocketServer(void) {
	if (dnsSocket != INVALID_SOCKET) {
		closesocket(dnsSocket);
		dnsSocket = INVALID_SOCKET;
	}

	WSACleanup();
}

void run_dns_server(void) {
	while (1) {
		uint8_t buffer[BUFFER_SIZE];
		struct sockaddr_in source_addr = { 0 };
		int source_addr_len = sizeof(source_addr);
		int msg_size = recvfrom(dnsSocket, (char*)buffer, sizeof(buffer), 0, (struct sockaddr*)&source_addr, &source_addr_len);

		if (msg_size == SOCKET_ERROR) {
			continue;
		}

		if (source_addr.sin_addr.s_addr == serverAddress.sin_addr.s_addr) {
			handle_remote_response(buffer, msg_size);
		}
		else {
			handle_client_request(buffer, msg_size, &source_addr, source_addr_len);
		}
	}
}
