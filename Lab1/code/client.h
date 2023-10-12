#pragma once
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")
const int BUFFSIZE = 1024;
int client(); //客户端程序
char* getServerIP(); //获取服务端的IP地址
DWORD WINAPI recvMessage_C(LPVOID lpParam); //客户端接受信息
