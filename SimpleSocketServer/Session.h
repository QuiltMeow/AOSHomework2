#pragma once

#define LENGTH_SIZE 4
#include <arpa/inet.h>
#include <atomic>
#include <mutex>
#include <string>

#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"
#include "WindowsCompatible.h"

class Session {
private:
    std::atomic<bool> pendingDisconnect;
    bool hasLength;
    int remaining;
    std::mutex sendMutex;
    std::mutex receiveMutex;
    SOCKADDR_IN ipAddress;
    SOCKET socket;

    PacketLittleEndianWriter buffer;
    PacketLittleEndianWriter message;

    void (*packetHandler)(Session&, BYTE*, int);

    void clearLength() {
        hasLength = false;
    }

    void store(PacketLittleEndianWriter& writer, const char* data, int length) {
        writer.write(data, length);
    }

    void processPacket() {
        unsigned char* packet = buffer.getPacket();
        PacketLittleEndianReader reader(packet, buffer.size());
        delete[] packet;

        while (reader.available() > 0) {
            if (!hasLength) {
                if (reader.available() < LENGTH_SIZE) {
                    break;
                }
                remaining = reader.readInt();
                hasLength = true;
            }

            int available = reader.available();
            if (available <= 0) {
                break;
            }

            int toRead = available < remaining ? available : remaining;
            if (toRead > 0) {
                unsigned char* read = reader.read(toRead);
                store(message, (const char*)read, toRead);
                delete[] read;
                remaining -= toRead;
            }

            if (remaining <= 0) {
                packetHandler(*this, message.getPacket(), message.size());
                message.clear();
                clearLength();
            }
        }

        buffer.clear();
        int last = reader.available();
        if (last > 0) {
            packet = reader.read(last);
            store(buffer, (const char*)packet, last);
            delete[] packet;
        }
    }
public:
    Session(SOCKET socket, SOCKADDR_IN ipAddress, void (*packetHandler)(Session&, BYTE*, int)) {
        pendingDisconnect = false;
        clearLength();
        this->socket = socket;
        this->ipAddress = ipAddress;
        this->packetHandler = packetHandler;
    }

    void receive(const char* data, int length) {
        std::lock_guard<std::mutex> lock(receiveMutex);
        store(buffer, data, length);
        processPacket();
    }

    bool isDisconnect() {
        return pendingDisconnect;
    }

    SOCKET getSocket() {
        return socket;
    }

    void disconnect() {
        pendingDisconnect = true;
    }

    void sendPacket(BYTE* data, int length) {
        std::lock_guard<std::mutex> lock(sendMutex);
        PacketLittleEndianWriter out;
        out.writeInt(length);
        out.write((const char*)data, length);

        BYTE* packet = out.getPacket();
        send(socket, (const char*)packet, out.size(), 0);
        delete[] packet;
    }

    SOCKADDR_IN getIPAddress() {
        return ipAddress;
    }

    static std::string getReadableIPAddress(SOCKADDR_IN address) {
        char ipString[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(address.sin_addr), ipString, INET_ADDRSTRLEN);
        return ipString + std::string(":") + std::to_string(address.sin_port);
    }

    std::string getReadableIPAddress() {
        return getReadableIPAddress(ipAddress);
    }
};
