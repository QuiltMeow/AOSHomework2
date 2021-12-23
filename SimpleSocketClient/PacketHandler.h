#pragma once

#include <iostream>
#include <pthread.h>
#include <mutex>
#include <fstream>
#include <condition_variable>
#include <algorithm>
#include <map>

#include "Session.h"
#include "Connector.h"
#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"
#include "WindowsCompatible.h"

#define BUFFER_SIZE 4096

SOCKET client = INVALID_SOCKET;
Session* session;
pthread_t applicationThreadHandle;

bool hasNotify = false;
std::condition_variable responseCondition;
std::mutex closeMutex;

bool invalidUserDisconnect = false;

void disconnect() {
    std::lock_guard<std::mutex> lock(closeMutex);
    if (client != INVALID_SOCKET) {
        closeSocket(client);
        client = INVALID_SOCKET;
    }
}

void stopApplicationThread() {
    disconnect();
    pthread_cancel(applicationThreadHandle);
}

LPVOID receiveThread(LPVOID lpParameter) {
    char buffer[BUFFER_SIZE];
    while (true) {
        int receiveLength = recv(client, buffer, sizeof(buffer), 0);
        if (receiveLength <= 0) {
            stopApplicationThread();
            system("clear");
            system("echo 中斷連線 ...");

            if (invalidUserDisconnect) {
                system("echo 無效的使用者名稱 : 使用者不存在");
            }
            break;
        }
        session->receive(buffer, receiveLength);
        if (session->isDisconnect()) {
            disconnect();
        }
    }
    return EXIT_SUCCESS;
}

void sendPacket(Session* target, PacketLittleEndianWriter& writer) {
    BYTE* packet = writer.getPacket();
    target->sendPacket(packet, writer.size());
    delete[] packet;
}

enum PacketOPCode {
    GET_HELLO = 0x00,
    CREATE_FILE = 0x01,
    READ_FILE = 0x02,
    WRITE_FILE = 0x03,
    CHANGE_MODE = 0x04,
    SHOW_CAPABILITY_MAP = 0x05,
    SHOW_FILE_LIST = 0x06
};

#define SUCCESS 0
#define ALLOW_READ_FILE 1
#define BAD_PERMISSION_STRING -1
#define FILE_EXIST -2
#define FILE_NOT_EXIST -3
#define FILE_OPEN_FAIL -4
#define ACCESS_DENIED -5
#define USER_NAME_NOT_EXIST -6
#define BAD_FILE_NAME -7
#define BAD_FILE_DATA -8

#define EMPTY_STRING ""

SOCKET sessionId;
map<int, string> errorMessage;

void initErrorMessage() {
    errorMessage[SUCCESS] = "操作順利完成";
    errorMessage[ALLOW_READ_FILE] = "允許讀取檔案";
    errorMessage[BAD_PERMISSION_STRING] = "無效的權限字串";
    errorMessage[FILE_EXIST] = "指定檔案已存在";
    errorMessage[FILE_NOT_EXIST] = "指定檔案不存在";
    errorMessage[FILE_OPEN_FAIL] = "檔案開啟失敗";
    errorMessage[ACCESS_DENIED] = "拒絕存取";
    errorMessage[USER_NAME_NOT_EXIST] = "指定使用者不存在";
    errorMessage[BAD_FILE_NAME] = "無效的檔案名稱";
    errorMessage[BAD_FILE_DATA] = "無效的檔案內容";
}

string getSterilizeFileName(string fileName) {
    replace(fileName.begin(), fileName.end(), '\\', '/');
    fileName.erase(remove(fileName.begin(), fileName.end(), '/'), fileName.end());
    return fileName;
}

string getErrorMessage(int status) {
    if (errorMessage.find(status) == errorMessage.end()) {
        return "未知錯誤";
    }
    return errorMessage[status];
}

void handlePacket(Session& session, BYTE* data, int length) {
    PacketLittleEndianReader packet(data, length);
    delete[] data;

    try {
        short op = packet.readShort();
        switch (op) {
        case GET_HELLO: {
            int status = packet.readInt();
            if (status != SUCCESS) {
                invalidUserDisconnect = true;
                break;
            }
            sessionId = packet.readInt();
            break;
        }
        case CREATE_FILE:
        case WRITE_FILE:
        case CHANGE_MODE: {
            int status = packet.readInt();
            cout << getErrorMessage(status) << endl;
            break;
        }
        case READ_FILE: {
            int status = packet.readInt();
            cout << getErrorMessage(status) << endl;

            if (status == ALLOW_READ_FILE) {
                string fileName = getSterilizeFileName(packet.readLengthAsciiString());
                if (fileName == EMPTY_STRING) {
                    cerr << errorMessage[BAD_FILE_NAME] << endl;
                    break;
                }

                pair<BYTE*, int> file = packet.readFile();
                if (file.first == NULL) {
                    cerr << errorMessage[BAD_FILE_DATA] << endl;
                    break;
                }

                fstream fs(fileName, ios::out | ios::binary);
                if (!fs) {
                    cerr << errorMessage[FILE_OPEN_FAIL] << endl;
                    delete[] file.first;
                    break;
                }
                if (file.second > 0) {
                    fs.write((const char*)file.first, file.second);
                }
                fs.close();
                delete[] file.first;
                cout << errorMessage[SUCCESS] << endl;
            }
            break;
        }
        case SHOW_CAPABILITY_MAP:
        case SHOW_FILE_LIST: {
            cout << packet.readLengthAsciiString();
            break;
        }
        }
    } catch (std::exception& ex) {
        std::cerr << "解析封包時發生例外狀況 : " << ex.what() << std::endl;
    }
    hasNotify = true;
    responseCondition.notify_all();
}
