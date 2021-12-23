#pragma once

#include "PacketLittleEndianWriter.h"
#include "Session.h"
#include "WindowsCompatible.h"
#include <map>
#include <mutex>

std::map<SOCKET, void*> clientMap;
std::mutex clientMapMutex;

bool attachClient(SOCKET socket, void* client) {
    std::lock_guard<std::mutex> lock(clientMapMutex);
    if (clientMap.find(socket) != clientMap.end()) {
        return false;
    }
    clientMap[socket] = client;
    return true;
}

void* getClient(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientMapMutex);
    if (clientMap.find(socket) == clientMap.end()) {
        return NULL;
    }
    return clientMap[socket];
}

bool detachClient(SOCKET socket) {
    std::lock_guard<std::mutex> lock(clientMapMutex);
    if (clientMap.find(socket) == clientMap.end()) {
        return false;
    }
    clientMap.erase(socket);
    return true;
}

void sendPacket(Session& session, PacketLittleEndianWriter& writer) {
    BYTE* packet = writer.getPacket();
    session.sendPacket(packet, writer.size());
    delete[] packet;
}
