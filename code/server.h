#pragma once
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")
const int BUFFMAX = 1024;
const int MAX_QUEUE = 5; //队列最长为5人
int server(); //服务器端程序
char* getLocalIP(); //输入通过ipconfig获取的IP地址
int findInQueue(); //查找是否有空位，没有则返回-1
DWORD WINAPI recvMessage_S(LPVOID lpParam); //服务端接受信息
struct clientInfo { //记录客户信息
    int id; // 队列编号
    char name[20];
};
