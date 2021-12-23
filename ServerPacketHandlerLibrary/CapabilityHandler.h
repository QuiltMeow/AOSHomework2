#pragma once

#include <fstream>
#include <map>
#include <sstream>
#include <mutex>
#include <shared_mutex>
#include <sys/stat.h>
#include <algorithm>

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

#define OWNER 0
#define GROUP 1
#define OTHER 2
#define PERMISSION_COUNT 3
#define PERMISSION_STRING_LENGTH 6

#define STRING_BUFFER_SIZE 200
#define EMPTY_STRING ""

#define ROLE_SIZE 6
#define GROUP_SIZE 2
#define DATA_DIRECTORY "./Data"

string roleName[ROLE_SIZE] = { "Quilt", "Neko", "Meow", "Nya", "Cat", "Kitten" };
string groupName[GROUP_SIZE] = { "AOSStudent", "CSEStudent" };

struct Permission {
    bool read, write;

    Permission() {
        read = write = false;
    }
};

struct Capability {
    string fileName;
    Permission permission;
};

struct User {
    string group;
    vector<Capability*> capabilityList;
};

struct FileInformation {
    Permission permission[PERMISSION_COUNT];
    string owner;
    string group;
    shared_mutex fileMutex;

    void parsePermission(string data) {
        for (int i = 0; i < PERMISSION_COUNT; ++i) {
            permission[i].read = data[i * 2] == 'r';
            permission[i].write = data[i * 2 + 1] == 'w';
        }
    }
};

// 沒有要存檔什麼的 不考慮指標記憶體洩漏問題
map<string, User*> capabilityMap;
map<string, FileInformation*> fileMap;
mutex createFileMutex;
shared_mutex mapMutex;

void initCapability() {
    for (int i = 0; i < ROLE_SIZE; ++i) {
        User* user = new User();
        user->group = groupName[i < ROLE_SIZE / 2 ? 0 : 1];
        capabilityMap[roleName[i]] = user;
    }
}

bool isUserNameExist(string userName) {
    for (int i = 0; i < ROLE_SIZE; ++i) {
        if (userName == roleName[i]) {
            return true;
        }
    }
    return false;
}

bool isFileExist(string path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0 && (buffer.st_mode & S_IFMT) == S_IFREG;
}

string getFileLastModifyDate(string path) {
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        return "無法查詢日期";
    }

    time_t time = buffer.st_mtime;
    struct tm* timeStruct = localtime(&time);

    char stringBuffer[STRING_BUFFER_SIZE];
    strftime(stringBuffer, STRING_BUFFER_SIZE, "%Y 年 %m 月 %d 日", timeStruct);
    return stringBuffer;
}

string getSterilizeFileName(string fileName) {
    replace(fileName.begin(), fileName.end(), '\\', '/');
    fileName.erase(remove(fileName.begin(), fileName.end(), '/'), fileName.end());
    return fileName;
}

string getSterilizeFilePath(string fileName) {
    return string(DATA_DIRECTORY) + "/" + getSterilizeFileName(fileName);
}

string getPermissionString(Permission* permission) {
    stringstream ss;
    for (int i = 0; i < PERMISSION_COUNT; ++i) {
        ss << (permission[i].read ? "r" : "-");
        ss << (permission[i].write ? "w" : "-");
    }
    return ss.str();
}

int getFileSize(string path) {
    fstream fs(path, ios::in | ios::binary);
    if (!fs) {
        return FILE_OPEN_FAIL;
    }
    fs.seekg(0, ios::end);
    int ret = fs.tellg();
    fs.close();
    return ret;
}

void createFile(Session& session, PacketLittleEndianWriter& writer, string fileName, string userName, string permission) {
    lock_guard<mutex> lock(createFileMutex); // 防止多個客戶端同時建立相同檔案名稱
    if (permission.length() != PERMISSION_STRING_LENGTH) {
        writer.writeInt(BAD_PERMISSION_STRING);
        sendPacket(session, writer);
        return;
    }

    string cleanFileName = getSterilizeFileName(fileName);
    if (cleanFileName == EMPTY_STRING) {
        writer.writeInt(BAD_FILE_NAME);
        sendPacket(session, writer);
        return;
    }

    string path = getSterilizeFilePath(fileName);
    if (isFileExist(path)) {
        writer.writeInt(FILE_EXIST);
        sendPacket(session, writer);
        return;
    }

    fstream fs(path, ios::out | ios::binary);
    if (!fs) {
        writer.writeInt(FILE_OPEN_FAIL);
        sendPacket(session, writer);
        return;
    }
    fs.close();

    unique_lock<shared_mutex> mapWriteLock(mapMutex);
    FileInformation* information = new FileInformation();
    information->parsePermission(permission);
    information->owner = userName;

    string userGroup = capabilityMap[userName]->group;
    information->group = userGroup;

    fileMap[cleanFileName] = information;
    cout << "建立檔案 : " << getPermissionString((Permission*)&(information->permission)) << " " << userName << " " << userGroup << " " << getFileLastModifyDate(path) << " " << cleanFileName << endl;

    for (int i = 0; i < ROLE_SIZE; ++i) {
        Capability* addCapability = new Capability();
        addCapability->fileName = cleanFileName;

        string role = roleName[i];
        User* user = capabilityMap[role];
        if (role == userName) {
            addCapability->permission.read = information->permission[OWNER].read;
            addCapability->permission.write = information->permission[OWNER].write;
        } else if (user->group == userGroup) {
            addCapability->permission.read = information->permission[GROUP].read;
            addCapability->permission.write = information->permission[GROUP].write;
        } else {
            addCapability->permission.read = information->permission[OTHER].read;
            addCapability->permission.write = information->permission[OTHER].write;
        }
        user->capabilityList.push_back(addCapability);
    }

    writer.writeInt(SUCCESS);
    sendPacket(session, writer);
}

void readFile(Session& session, PacketLittleEndianWriter& writer, string fileName, string userName) {
    string cleanFileName = getSterilizeFileName(fileName);
    if (cleanFileName == EMPTY_STRING) {
        writer.writeInt(BAD_FILE_NAME);
        sendPacket(session, writer);
        return;
    }

    string path = getSterilizeFilePath(fileName);
    if (!isFileExist(path)) {
        writer.writeInt(FILE_NOT_EXIST);
        sendPacket(session, writer);
        return;
    }

    shared_lock<shared_mutex> mapReadLock(mapMutex);
    User* user = capabilityMap[userName];
    int length = user->capabilityList.size();
    for (int i = 0; i < length; ++i) {
        Capability* capability = user->capabilityList[i];
        if (capability->fileName == cleanFileName) {
            if (!capability->permission.read) {
                writer.writeInt(ACCESS_DENIED);
                sendPacket(session, writer);
                return;
            }
            break;
        }
    }

    FileInformation* information = fileMap[cleanFileName];
    shared_lock<shared_mutex> fileReadLock(information->fileMutex);

    cout << "使用者 " << userName << " 讀取檔案 " << cleanFileName << endl;
    writer.writeInt(ALLOW_READ_FILE);
    writer.writeLengthAsciiString(cleanFileName);
    writer.writeFile(path);
    sendPacket(session, writer);
}

void writeFile(Session& session, PacketLittleEndianWriter& writer, string fileName, string userName, bool overwrite, BYTE* data, int dataLength) {
    string cleanFileName = getSterilizeFileName(fileName);
    if (cleanFileName == EMPTY_STRING) {
        writer.writeInt(BAD_FILE_NAME);
        sendPacket(session, writer);
        return;
    }

    string path = getSterilizeFilePath(fileName);
    if (!isFileExist(path)) {
        writer.writeInt(FILE_NOT_EXIST);
        sendPacket(session, writer);
        return;
    }

    shared_lock<shared_mutex> mapReadLock(mapMutex);
    User* user = capabilityMap[userName];
    int length = user->capabilityList.size();
    for (int i = 0; i < length; ++i) {
        Capability* capability = user->capabilityList[i];
        if (capability->fileName == cleanFileName) {
            if (!capability->permission.write) {
                writer.writeInt(ACCESS_DENIED);
                sendPacket(session, writer);
                return;
            }
            break;
        }
    }

    FileInformation* information = fileMap[cleanFileName];
    unique_lock<shared_mutex> fileWriteLock(information->fileMutex);

    fstream fs;
    if (overwrite) {
        fs.open(path, ios::out | ios::binary);
    } else {
        fs.open(path, ios::out | ios::app | ios::binary);
    }
    if (!fs) {
        writer.writeInt(FILE_OPEN_FAIL);
        sendPacket(session, writer);
        return;
    }

    cout << "使用者 " << userName << " 寫入檔案 " << cleanFileName << " " << (overwrite ? "覆寫" : "附加") << "模式" << endl;
    if (dataLength > 0) {
        fs.write((const char*)data, dataLength);
    }
    fs.close();

    writer.writeInt(SUCCESS);
    sendPacket(session, writer);
}

void changeMode(Session& session, PacketLittleEndianWriter& writer, string fileName, string userName, string permission) {
    if (permission.length() != PERMISSION_STRING_LENGTH) {
        writer.writeInt(BAD_PERMISSION_STRING);
        sendPacket(session, writer);
        return;
    }

    string cleanFileName = getSterilizeFileName(fileName);
    if (cleanFileName == EMPTY_STRING) {
        writer.writeInt(BAD_FILE_NAME);
        sendPacket(session, writer);
        return;
    }

    string path = getSterilizeFilePath(fileName);
    if (!isFileExist(path)) {
        writer.writeInt(FILE_NOT_EXIST);
        sendPacket(session, writer);
        return;
    }

    unique_lock<shared_mutex> mapWriteLock(mapMutex);
    FileInformation* information = fileMap[cleanFileName];
    if (information->owner != userName) {
        writer.writeInt(ACCESS_DENIED);
        sendPacket(session, writer);
        return;
    }

    information->parsePermission(permission);
    string userGroup = information->group;
    cout << "使用者 " << userName << " 變更檔案 " << cleanFileName << " 權限 " << getPermissionString((Permission*)&(information->permission)) << endl;

    for (int i = 0; i < ROLE_SIZE; ++i) {
        string role = roleName[i];
        User* user = capabilityMap[role];

        int length = user->capabilityList.size();
        for (int j = 0; j < length; ++j) {
            Capability* capability = user->capabilityList[j];
            if (capability->fileName == cleanFileName) {
                if (role == userName) {
                    capability->permission.read = information->permission[OWNER].read;
                    capability->permission.write = information->permission[OWNER].write;
                } else if (user->group == userGroup) {
                    capability->permission.read = information->permission[GROUP].read;
                    capability->permission.write = information->permission[GROUP].write;
                } else {
                    capability->permission.read = information->permission[OTHER].read;
                    capability->permission.write = information->permission[OTHER].write;
                }
                break;
            }
        }
    }

    writer.writeInt(SUCCESS);
    sendPacket(session, writer);
}

string getCapabilityMapString() {
    stringstream ss;
    shared_lock<shared_mutex> mapReadLock(mapMutex);
    for (int i = 0; i < ROLE_SIZE; ++i) {
        string role = roleName[i];
        ss << role << " ";

        User* user = capabilityMap[role];
        ss << user->group << " :" << endl;

        int length = user->capabilityList.size();
        for (int j = 0; j < length; ++j) {
            Capability* capability = user->capabilityList[j];
            ss << capability->fileName << " ";
            ss << (capability->permission.read ? "r" : "-");
            ss << (capability->permission.write ? "w" : "-") << endl;
        }
        ss << endl;
    }
    return ss.str();
}

string getFileMapString() {
    stringstream ss;
    shared_lock<shared_mutex> mapReadLock(mapMutex);
    for (map<string, FileInformation*>::iterator itr = fileMap.begin(); itr != fileMap.end(); ++itr) {
        string fileName = itr->first;
        string path = getSterilizeFilePath(fileName);
        int fileSize = getFileSize(path);

        FileInformation* information = itr->second;
        ss << getPermissionString((Permission*)&(information->permission)) << " " << information->owner << " " << information->group << " ";
        if (fileSize != FILE_OPEN_FAIL) {
            ss << fileSize;
        } else {
            ss << "無法取得檔案大小";
        }
        ss << " " << getFileLastModifyDate(path) << " " << fileName << endl;
    }
    return ss.str();
}
