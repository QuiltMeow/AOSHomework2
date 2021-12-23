#pragma once

#include <iostream>
#include <unistd.h>
#include <netinet/tcp.h>

#include "WindowsCompatible.h"

#define KEEP_ALIVE_TIME 60
#define KEEP_ALIVE_INTERVAL 5

void closeSocket(int socketFD) {
    shutdown(socketFD, SHUT_RDWR);
    close(socketFD);
}

SOCKET connectServer(SOCKADDR_IN target) {
    SOCKET ret = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ret == INVALID_SOCKET) {
        std::cerr << "無效的 Socket" << std::endl;
        return INVALID_SOCKET;
    }

    BOOL enable = TRUE;
    if (setsockopt(ret, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)) != 0) {
        std::cerr << "Socket 無法設定 Keep Alive 狀態" << std::endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }
    if (setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) != 0) {
        std::cerr << "Socket 無法設定 No Delay 狀態" << std::endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    int keepAliveTime = KEEP_ALIVE_TIME;
    if (setsockopt(ret, IPPROTO_TCP, TCP_KEEPIDLE, &keepAliveTime, sizeof(keepAliveTime)) != 0) {
        std::cerr << "Socket 無法設定 Keep Alive 時間" << std::endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    int keepAliveInterval = KEEP_ALIVE_INTERVAL;
    if (setsockopt(ret, IPPROTO_TCP, TCP_KEEPINTVL, &keepAliveInterval, sizeof(keepAliveInterval)) != 0) {
        std::cerr << "Socket 無法設定 Keep Alive 間隔" << std::endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    std::cout << "正在連線到伺服器 ..." << std::endl;
    if (connect(ret, (SOCKADDR*)&target, sizeof(target)) != 0) {
        std::cerr << "無法與伺服器建立連線 ..." << std::endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }
    return ret;
}
