#include "PacketLittleEndianReader.h"
#include "PacketLittleEndianWriter.h"
#include "WindowsCompatible.h"
#include "SessionLib.h"
#include "Session.h"

#include <iostream>
#include <filesystem>

using namespace std;

#include "CapabilityHandler.h"
#include "Client.h"

void packetHandler(Session&, BYTE*, int);

extern "C" {
    void* setupPacketReceive();
    void* setupClientConnect();
    void* setupClientDisconnect();
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

bool onClientConnect(SOCKET socket, SOCKADDR_IN address) {
    Client* client = new Client();
    attachClient(socket, client);
    return true;
}

void onClientDisconnect(SOCKET socket) {
    Client* client = (Client*)getClient(socket);
    delete client;
    detachClient(socket);
}

void packetHandler(Session& session, BYTE* data, int length) {
    PacketLittleEndianReader packet(data, length);
    delete[] data;

    Client* client = (Client*)getClient(session.getSocket());
    try {
        short op = packet.readShort();

        PacketLittleEndianWriter writer;
        writer.writeShort(op);
        switch (op) {
        case GET_HELLO: {
            string userName = packet.readLengthAsciiString();
            if (!isUserNameExist(userName)) {
                writer.writeInt(USER_NAME_NOT_EXIST);
                sendPacket(session, writer);
                session.disconnect();
                break;
            }

            client->userName = userName;
            writer.writeInt(SUCCESS);
            writer.writeInt(session.getSocket());
            sendPacket(session, writer);
            break;
        }
        case CREATE_FILE: {
            string userName = client->userName;
            if (userName == EMPTY_STRING) {
                session.disconnect();
                break;
            }
            string fileName = packet.readLengthAsciiString();
            string permission = packet.readLengthAsciiString();
            createFile(session, writer, fileName, userName, permission);
            break;
        }
        case READ_FILE: {
            string userName = client->userName;
            if (userName == EMPTY_STRING) {
                session.disconnect();
                break;
            }
            string fileName = packet.readLengthAsciiString();
            readFile(session, writer, fileName, userName);
            break;
        }
        case WRITE_FILE: {
            string userName = client->userName;
            if (userName == EMPTY_STRING) {
                session.disconnect();
                break;
            }
            string fileName = packet.readLengthAsciiString();
            bool overwrite = packet.readByte();
            pair<BYTE*, int> file = packet.readFile();
            if (file.first == NULL) {
                writer.writeInt(BAD_FILE_DATA);
                sendPacket(session, writer);
                break;
            }
            writeFile(session, writer, fileName, userName, overwrite, file.first, file.second);
            delete[] file.first;
            break;
        }
        case CHANGE_MODE: {
            string userName = client->userName;
            if (userName == EMPTY_STRING) {
                session.disconnect();
                break;
            }
            string fileName = packet.readLengthAsciiString();
            string permission = packet.readLengthAsciiString();
            changeMode(session, writer, fileName, userName, permission);
            break;
        }
        case SHOW_CAPABILITY_MAP: {
            writer.writeLengthAsciiString(getCapabilityMapString());
            sendPacket(session, writer);
            break;
        }
        case SHOW_FILE_LIST: {
            writer.writeLengthAsciiString(getFileMapString());
            sendPacket(session, writer);
            break;
        }
        }
    } catch (exception& ex) {
        cerr << "解析封包時發生例外狀況 : " << ex.what() << endl;
    }
}

void deleteDataContent() {
    for (const filesystem::directory_entry& entry : filesystem::directory_iterator(DATA_DIRECTORY)) {
        filesystem::remove_all(entry.path());
    }
}

void* setupPacketReceive() {
    cout << "[注意] 您可以將資料夾內檔案複製到外部進行備份，但不應該直接修改資料夾內部內容，每次程式啟動時自動清除舊檔案" << endl;
    cout << "正在清除資料夾內檔案 ..." << endl;
    deleteDataContent();

    initCapability();
    return (void*)packetHandler;
}

void* setupClientConnect() {
    return (void*)onClientConnect;
}

void* setupClientDisconnect() {
    return (void*)onClientDisconnect;
}
