#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unordered_map>
#include "client.h"

#define SERVER_PORT 50001            // 公网服务器的端口
#define SERVER_ADDR "8.140.237.61"   // 公网服务器的IP地址
#define BUF_SIZE 1024                // 缓冲区大小(字节)
#define KEEP_ALIVE_INTERVAL 20       // 保活包发送间隔

struct NatAddress {
    std::string ip;
    int port;
};
std::unordered_map<std::string, NatAddress> nat_addrs;
std::unordered_map<std::string, int> client_sock_fds;

void parseAddress(char* buffer, int buffer_len) {
    std::string str(buffer, buffer_len);
    int start_pos = 0, end_pos;
    while ((end_pos = str.find(' ', start_pos)) != std::string::npos) {
        std::string client_name = str.substr(start_pos, end_pos - start_pos);

        start_pos = end_pos + 1;
        end_pos = str.find(' ', start_pos);
	std::string ip = str.substr(start_pos, end_pos - start_pos);

        start_pos = end_pos + 1;
        end_pos = str.find(' ', start_pos);
        std::string port = str.substr(start_pos, end_pos - start_pos);

        NatAddress nat_addr;
        nat_addr.ip = ip;
        nat_addr.port = std::stoi(port);
        nat_addrs[client_name] = nat_addr;

	if (end_pos == std::string::npos) break;
        start_pos = end_pos + 1;
    }
}

bool dig_hole(const std::string& client_name) {
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_ADDR);
    server_addr.sin_port = htons(SERVER_PORT);

    int client_sock_fd = 0;
    auto it = client_sock_fds.find(client_name);
    if (it == client_sock_fds.end()) {
        client_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (client_sock_fd < 0)
        {
            printf("create socket failed, errno:%d, error:%s\n", errno, strerror(errno));
            return false;
        }
        client_sock_fds[client_name] = client_sock_fd;
    } else {
        client_sock_fd = it->second;
    }

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
    return true;
}

bool fill_hole(const std::string& client_name) {
    auto it = client_sock_fds.find(client_name);
    if (it != client_sock_fds.end()) {
        close(it->second);
        client_sock_fds.erase(it);
        return true;
    }
    return false;
}

bool fill_all_holes() {
    for (auto it = client_sock_fds.begin(); it != client_sock_fds.end(); it++) {
        close(it->second);
    }
    client_sock_fds.clear();
    return true;
}

bool get_address_by_name(const std::string& client_name, std::string& ip, int& port) {
    auto it = nat_addrs.find(client_name);
    if (it == nat_addrs.end()) {
        printf("client %s not exist in map, try again\n", client_name.c_str());
        return false;
    }
    ip = it->second.ip;
    port = it->second.port;
    return true;
}
