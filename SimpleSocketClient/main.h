#pragma once

#include <iostream>
#include <mutex>
#include <sys/stat.h>
#include <pthread.h>

#include "Session.h"
#include "PacketLittleEndianWriter.h"
#include "Connector.h"
#include "WindowsCompatible.h"

#define CONNECT_IP_ADDRESS "127.0.0.1"
#define CONNECT_PORT 8923
#define PAUSE cout << "請按 Enter 鍵繼續 ..."; system("stty -echo"); cin.ignore(numeric_limits<streamsize>::max(), '\n'); system("stty echo");
#define SPACE " "
#define BAD_COMMAND_FORMAT "指令格式錯誤"

using namespace std;

#include "PacketHandler.h"

mutex response;
void waitResponse();

void sendHello(string);
void sendCreateFile(string, string);
void sendReadFile(string);
void sendWriteFile(string, bool);
void sendChangeMode(string, string);
void queryCapability();
void queryFile();

vector<string> split(string, string);
LPVOID uiThread(LPVOID lpParamter);
bool isFileExist(string);
