#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include "client.h"

#define SERVER_PORT 50001            // 公网服务器的端口
#define SERVER_ADDR "8.140.237.61"   // 公网服务器的IP地址
#define BUF_SIZE 1024                // 缓冲区大小(字节)
#define KEEP_ALIVE_INTERVAL 20       // 保活包发送间隔

static bool is_init = false;
static std::string peer_nat_ip;
static int peer_nat_port = 0;

void parseAddress(char* buffer, int buffer_len) {
    char name[64];
    char ip[32];
    int port = 0;
    sscanf(buffer, "%s %s %d", name, ip, &port);
    peer_nat_ip = ip;
    peer_nat_port = port;
}

bool init(const std::string& client_name) {
    if (is_init) {
        return true;
    }

    int client_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_sock_fd < 0)
    {
        printf("create socket failed, errno:%d, error:%s\n", errno, strerror(errno));
        return false;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);

    socklen_t addr_len = sizeof(struct sockaddr_in);
    ssize_t sent_bytes = sendto(client_sock_fd,
                                client_name.c_str(),
                                client_name.length(),
                                MSG_CONFIRM,
                                (const struct sockaddr *)&server_addr,
                                addr_len);
    if (sent_bytes == -1)
    {
        printf("send failed, errno:%d, error:%s\n", errno, strerror(errno));
        close(client_sock_fd);
        return false;
    }
    printf("wait for reply from server.\n");

    char buffer[BUF_SIZE];
    memset(buffer, 0, BUF_SIZE);
    int n = recvfrom(client_sock_fd,
                    buffer,
                    BUF_SIZE,
                    0,
                    (struct sockaddr *)&server_addr,
                    &addr_len);
    buffer[n] = '\0';
    printf("dig hole, recv from server: %s\n", buffer);
    parseAddress(buffer, n+1);
    is_init = true;
    return true;
}

bool getPeerAddr(const char* client_name, std::string& ip, int& port) {
    if (!is_init) {
        init(client_name);
    }

    ip = peer_nat_ip;
    port = peer_nat_port;
    return true;
}
