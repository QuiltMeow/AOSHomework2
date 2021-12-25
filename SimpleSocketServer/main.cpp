#include "main.h"

int main() {
    cout << "初始化封包處理器 ..." << endl;
    HINSTANCE library = dlopen("./PacketHandler.so", RTLD_LAZY);
    if (library == NULL) {
        cerr << "封包處理器載入失敗" << endl;
        return EXIT_FAILURE;
    }
    void* (*setupPacketReceive)() = (void* (*)()) dlsym(library, "setupPacketReceive");
    if (setupPacketReceive == NULL) {
        cerr << "無法取得封包處理器載入函式" << endl;
        return EXIT_FAILURE;
    }
    onPacketReceive = (void(*)(Session&, BYTE*, int)) setupPacketReceive();
    if (onPacketReceive == NULL) {
        cerr << "封包處理器載入函式實作錯誤" << endl;
        return EXIT_FAILURE;
    }

    void* (*setupClientConnect)() = (void* (*)()) dlsym(library, "setupClientConnect");
    if (setupClientConnect != NULL) {
        cout << "註冊客戶端連線事件" << endl;
        onClientConnect = (bool(*)(SOCKET, SOCKADDR_IN)) setupClientConnect();
    }

    void* (*setupClientDisconnect)() = (void* (*)()) dlsym(library, "setupClientDisconnect");
    if (setupClientDisconnect != NULL) {
        cout << "註冊客戶端斷線事件" << endl;
        onClientDisconnect = (void(*)(SOCKET)) setupClientDisconnect();
    }

    void* (*setupUserServerClose)() = (void* (*)()) dlsym(library, "setupUserServerClose");
    if (setupUserServerClose != NULL) {
        cout << "註冊使用者關閉服務端事件" << endl;
        onUserServerClose = (void(*)()) setupUserServerClose();
    }

    SOCKET init = initServer();
    if (init == INVALID_SOCKET) {
        return EXIT_FAILURE;
    }
    server = init;

    cout << "建立接受連線執行緒 ..." << endl;
    pthread_t acceptThreadHandle;
    int	threadCreateResult = pthread_create(&acceptThreadHandle, NULL, acceptThread, NULL);
    if (threadCreateResult != 0) {
        cerr << "接受連線執行緒建立失敗" << endl;
        return EXIT_FAILURE;
    }

    cout << "按下 Enter 按鍵結束程式" << endl;
    system("stty -echo");
    cin.ignore(numeric_limits<streamsize>::max(), '\n');
    system("stty echo");

    cout << "關閉服務端 ..." << endl;
    if (onUserServerClose != NULL) {
        onUserServerClose();
    }
    return EXIT_SUCCESS;
}

void closeSocket(SOCKET socketFD) {
    shutdown(socketFD, SHUT_RDWR);
    close(socketFD);
}

SOCKET initServer() {
    SOCKET ret = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ret == INVALID_SOCKET) {
        cerr << "無效的 Socket" << endl;
        return INVALID_SOCKET;
    }

    BOOL enable = TRUE;
    int enableSize = sizeof(enable);
    if (setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, &enable, enableSize) != 0) {
        cerr << "Socket 無法設定重複使用狀態" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    if (setsockopt(ret, SOL_SOCKET, SO_KEEPALIVE, &enable, enableSize) != 0) {
        cerr << "Socket 無法設定 Keep Alive 狀態" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }
    if (setsockopt(ret, IPPROTO_TCP, TCP_NODELAY, &enable, enableSize) != 0) {
        cerr << "Socket 無法設定 No Delay 狀態" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    int keepAliveTime = KEEP_ALIVE_TIME;
    if (setsockopt(ret, IPPROTO_TCP, TCP_KEEPIDLE, &keepAliveTime, sizeof(keepAliveTime)) != 0) {
        cerr << "Socket 無法設定 Keep Alive 時間" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    int keepAliveInterval = KEEP_ALIVE_INTERVAL;
    if (setsockopt(ret, IPPROTO_TCP, TCP_KEEPINTVL, &keepAliveInterval, sizeof(keepAliveInterval)) != 0) {
        cerr << "Socket 無法設定 Keep Alive 間隔" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    SOCKADDR_IN local;
    bzero(&local, sizeof(local));
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_family = AF_INET;
    local.sin_port = htons(PORT);
    if (bind(ret, (SOCKADDR*)&local, sizeof(local)) != 0) {
        cerr << "服務端端口綁定失敗" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }

    if (listen(ret, MAX_PENDING_CONNECTION) != 0) {
        cerr << "服務端端口監聽失敗" << endl;
        closeSocket(ret);
        return INVALID_SOCKET;
    }
    return ret;
}

LPVOID acceptThread(LPVOID lpParameter) {
    SOCKADDR_IN clientAddress;
    socklen_t clientAddressLength = sizeof(clientAddress);

    cout << "服務端就緒 準備接受連線" << endl;
    while (run) {
        SOCKET client = accept(server, (SOCKADDR*)&clientAddress, &clientAddressLength);
        if (client == INVALID_SOCKET) {
            continue;
        }
        if (clientCount >= MAX_CONNECT_CLIENT) {
            closeSocket(client);
            continue;
        }

        SOCKET* parameter = new SOCKET(client);
        pthread_t receiveThreadHandle;
        if (pthread_create(&receiveThreadHandle, NULL, receiveThread, parameter) != 0) {
            closeSocket(client);
            delete parameter;
            continue;
        }

        socketMutex.lock();
        if (onClientConnect != NULL) {
            if (!onClientConnect(client, clientAddress)) {
                socketMutex.unlock();
                closeSocket(client);
                continue;
            }
        }

        char ipString[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddress.sin_addr), ipString, INET_ADDRSTRLEN);
        cout << "[接受連線] IP : " << ipString << ":" << clientAddress.sin_port << " 已連線到您的伺服器" << endl;
        sessionMap[client] = new Session(client, clientAddress, onPacketReceive);
        ++clientCount;
        socketMutex.unlock();
    }
    return EXIT_SUCCESS;
}

void removeClient(SOCKET client) {
    lock_guard<mutex> lock(socketMutex);
    closeSocket(client);
    delete sessionMap[client];
    sessionMap.erase(client);
    --clientCount;

    if (onClientDisconnect != NULL) {
        onClientDisconnect(client);
    }
}

LPVOID receiveThread(LPVOID lpParameter) {
    SOCKET* parameterPointer = (SOCKET*) lpParameter;
    SOCKET client = *parameterPointer;
    delete parameterPointer;

    socketMutex.lock();
    Session* session = sessionMap[client];
    socketMutex.unlock();

    char buffer[BUFFER_SIZE];
    while (run) {
        int receiveLength = recv(client, buffer, sizeof(buffer), 0);
        if (receiveLength <= 0) {
            removeClient(client);
            break;
        }

        session->receive(buffer, receiveLength);
        if (session->isDisconnect()) {
            removeClient(client);
            break;
        }
    }
    return EXIT_SUCCESS;
}
