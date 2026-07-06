#include "DNS_config.h"
#include "DNS_server.h"

int main(int argc, char* argv[]) {
    init(argc, argv);
    run_dns_server();
    closeSocketServer();
    return 0;
}
