#include <Winsock2.h>
#include <stdio.h>
#include <stdlib.h>
#include <cassert>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <string>
#include <chrono>

#define CLIENT_PORT 50001      // 客户端的端口
#define BUF_SIZE 1024          // 缓冲区大小(字节)
#define KEEP_ALIVE_INTERVAL 20 // 保活包发送间隔

std::unordered_map<std::string, std::string> nat_addrs;


int main() {
    WSADATA wsaData;
    SOCKET server_sock_fd;
    struct sockaddr_in server_addr;

    //检查协议栈
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Failed to load Winsock.\n");
        return -1;
    }

    //建立监听socket
    server_sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_sock_fd == INVALID_SOCKET) {
        printf("socket() failed:%d\n", WSAGetLastError());
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CLIENT_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(server_sock_fd, (LPSOCKADDR)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("bind() failed:%d\n", WSAGetLastError());
        return -1;
    }

    while (true) {
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        char buffer[BUF_SIZE];
        int len;
        printf("listening from clinet.\n");
        len = recvfrom(server_sock_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_addr, &client_addr_len);
        if (len == SOCKET_ERROR) {
            printf("recv() failed:%d\n", WSAGetLastError());
            continue;
        }
        buffer[len] = '\0';

        printf("recv client buffer len: %d, buffer: %s, IP:[%s],PORT:[%d]\n",
            len, buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        char nat_msg[BUF_SIZE];
        memset(nat_msg, 0, BUF_SIZE);
        sprintf(nat_msg, "%s %s %d", buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        std::string client_name(buffer, len + 1);
        nat_addrs[client_name] = std::string(nat_msg);

        std::string reply;
        for (auto it = nat_addrs.begin(); it != nat_addrs.end(); it++) {
            if (it != nat_addrs.begin()) {
                reply += " ";
            }
            reply += it->second;
        }
        sendto(server_sock_fd, reply.c_str(), reply.length(), 0, (struct sockaddr*)&client_addr, client_addr_len);
        printf("reply: %s\n", reply.c_str());
    }
    closesocket(server_sock_fd);
    WSACleanup();

    return 0;
}
