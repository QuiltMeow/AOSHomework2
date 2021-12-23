#pragma once

#include <iostream>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <vector>
#include <map>
#include <mutex>

#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"
#include "Session.h"
#include "WindowsCompatible.h"

#define PORT 8923
#define MAX_PENDING_CONNECTION 1000
#define BUFFER_SIZE 4096
#define MAX_CONNECT_CLIENT 200

// Keep Alive 時間 (單位 : 秒)
#define KEEP_ALIVE_TIME 60
#define KEEP_ALIVE_INTERVAL 5

using namespace std;

volatile atomic<bool> run = true;
int clientCount = 0;
map<SOCKET, Session*> sessionMap;
mutex socketMutex;

SOCKET server;

bool (*onClientConnect)(SOCKET, SOCKADDR_IN) = NULL;
void (*onPacketReceive)(Session&, BYTE*, int);
void (*onClientDisconnect)(SOCKET) = NULL;
void (*onUserServerClose)() = NULL;

LPVOID acceptThread(LPVOID);
LPVOID receiveThread(LPVOID);
void removeClient(SOCKET);
SOCKET initServer();
void closeSocket(SOCKET);
