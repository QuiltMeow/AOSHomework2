#include "main.h"

int main() {
    SOCKADDR_IN target;
    bzero(&target, sizeof(target));
    inet_pton(AF_INET, CONNECT_IP_ADDRESS, &(target.sin_addr));
    target.sin_family = AF_INET;
    target.sin_port = htons(CONNECT_PORT);

    client = connectServer(target);
    if (client == INVALID_SOCKET) {
        return EXIT_FAILURE;
    }
    session = new Session(client, target, handlePacket);

    int	threadCreateResult = pthread_create(&applicationThreadHandle, NULL, uiThread, NULL);
    if (threadCreateResult != 0) {
        cerr << "應用執行緒建立失敗" << endl;
        disconnect();
        return EXIT_FAILURE;
    }

    pthread_t receiveThreadHandle;
    threadCreateResult = pthread_create(&receiveThreadHandle, NULL, receiveThread, NULL);
    if (threadCreateResult != 0) {
        stopApplicationThread();
        system("clear");
        system("echo 收包執行緒建立失敗");
        return EXIT_FAILURE;
    }

    pthread_join(applicationThreadHandle, NULL);
    disconnect();
    pthread_join(receiveThreadHandle, NULL);
    return EXIT_SUCCESS;
}

void waitResponse() {
    unique_lock<mutex> lock(response);
    responseCondition.wait(lock, [] {
        return hasNotify;
    });
    hasNotify = false;
}

bool isFileExist(string path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0 && (buffer.st_mode & S_IFMT) == S_IFREG;
}

vector<string> split(string input, string pattern) {
    size_t patternSize = pattern.size();

    string::size_type pos;
    vector<string> ret;
    input += pattern;
    size_t size = input.size();
    for (size_t i = 0; i < size; ++i) {
        pos = input.find(pattern, i);
        if (pos < size) {
            string handle = input.substr(i, pos - i);
            ret.push_back(handle);
            i = pos + patternSize - 1;
        }
    }
    return ret;
}

void sendHello(string userName) {
    PacketLittleEndianWriter writer;
    writer.writeShort(GET_HELLO);
    writer.writeLengthAsciiString(userName);
    sendPacket(session, writer);
}

void sendCreateFile(string fileName, string permission) {
    PacketLittleEndianWriter writer;
    writer.writeShort(CREATE_FILE);
    writer.writeLengthAsciiString(fileName);
    writer.writeLengthAsciiString(permission);
    sendPacket(session, writer);
}

void sendReadFile(string fileName) {
    PacketLittleEndianWriter writer;
    writer.writeShort(READ_FILE);
    writer.writeLengthAsciiString(fileName);
    sendPacket(session, writer);
}

void sendWriteFile(string fileName, bool overwrite) {
    PacketLittleEndianWriter writer;
    writer.writeShort(WRITE_FILE);
    writer.writeLengthAsciiString(fileName);
    writer.writeBool(overwrite);
    writer.writeFile(fileName);
    sendPacket(session, writer);
}

void sendChangeMode(string fileName, string permission) {
    PacketLittleEndianWriter writer;
    writer.writeShort(CHANGE_MODE);
    writer.writeLengthAsciiString(fileName);
    writer.writeLengthAsciiString(permission);
    sendPacket(session, writer);
}

void queryCapability() {
    PacketLittleEndianWriter writer;
    writer.writeShort(SHOW_CAPABILITY_MAP);
    sendPacket(session, writer);
}

void queryFile() {
    PacketLittleEndianWriter writer;
    writer.writeShort(SHOW_FILE_LIST);
    sendPacket(session, writer);
}

LPVOID uiThread(LPVOID lpParameter) {
    initErrorMessage();

    string userName;
    cout << "請輸入使用者名稱 : ";
    getline(cin, userName);

    sendHello(userName);
    waitResponse();

    string input;
    while (true) {
        system("clear");
        cout << "===== Simple File Access Client =====" << endl;
        cout << "登入使用者 : " << userName << "，Session 代碼 : " << sessionId << endl;
        cout << "create [檔案名稱] [權限] 建立遠端檔案" << endl;
        cout << "read [檔案名稱] 下載遠端檔案" << endl;
        cout << "write [檔案名稱] [o 覆寫 / a 附加] 上傳檔案到遠端" << endl;
        cout << "changemode [檔案名稱] [權限] 變更遠端檔案權限" << endl;
        cout << "capability 顯示遠端 Capability 列表" << endl;
        cout << "file 顯示遠端檔案列表" << endl;
        cout << "exit 關閉連線" << endl;
        cout << "權限格式 [擁有者] [群組] [其他] r 可讀 w 可寫 - 無權 (範例 : rwr---)" << endl;
        cout << "===== Simple File Access Client =====" << endl << endl;

        cout << "請輸入指令 : ";
        getline(cin, input);
        cout << endl;

        if (input == EMPTY_STRING) {
            cerr << "沒有輸入任何指令" << endl << endl;
            PAUSE
            continue;
        }

        vector<string> command = split(input, SPACE);
        string operation = command[0];
        if (operation == "create") {
            if (command.size() < 3) {
                cerr << BAD_COMMAND_FORMAT << endl << endl;
                PAUSE
                continue;
            }
            string fileName = command[1];
            string permission = command[2];
            sendCreateFile(fileName, permission);
            waitResponse();
        } else if (operation == "read") {
            if (command.size() < 2) {
                cerr << BAD_COMMAND_FORMAT << endl << endl;
                PAUSE
                continue;
            }
            string fileName = command[1];
            sendReadFile(fileName);
            waitResponse();
        } else if (operation == "write") {
            if (command.size() < 3) {
                cerr << BAD_COMMAND_FORMAT << endl << endl;
                PAUSE
                continue;
            }
            string fileName = getSterilizeFileName(command[1]);
            if (fileName == EMPTY_STRING) {
                cerr << "請輸入有效檔案名稱" << endl << endl;
                PAUSE
                continue;
            }
            if (!isFileExist(fileName)) {
                cerr << "檔案不存在" << endl << endl;
                PAUSE
                continue;
            }
            bool overwrite = command[2] != "a";
            sendWriteFile(fileName, overwrite);
            waitResponse();
        } else if (operation == "changemode") {
            if (command.size() < 3) {
                cerr << BAD_COMMAND_FORMAT << endl << endl;
                PAUSE
                continue;
            }
            string fileName = command[1];
            string permission = command[2];
            sendChangeMode(fileName, permission);
            waitResponse();
        } else if (operation == "capability") {
            queryCapability();
            waitResponse();
        } else if (operation == "file") {
            queryFile();
            waitResponse();
        } else if (operation == "exit") {
            return EXIT_SUCCESS;
        } else {
            cerr << "無法辨認的指令內容" << endl;
        }

        cout << endl;
        PAUSE
    }
    return EXIT_SUCCESS;
}
