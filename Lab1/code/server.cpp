#include "server.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#pragma warning(disable:4996)
using namespace std;

int LOCAL_PORT = 19191;
bool QUIT = false; // 服务器是否退出
int CLIENT_NUMBER = 0; //有多少人在使用服务器
bool online[MAX_QUEUE] = { false }; //服务队列有效位
HANDLE client_threads[MAX_QUEUE];
clientInfo ClientInfo[MAX_QUEUE];
char recvBuff_S[BUFFMAX];
char sendBuff_S[BUFFMAX];
// 创建用户队列
SOCKET client_sockets[MAX_QUEUE];
SOCKADDR_IN clientAddrs[MAX_QUEUE];

int server()
{
    // 初始化 winsock库
    WSADATA wsadata;
    int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (res != 0) {
        WSAGetLastError();
        system("pause"); //出错不立即关闭窗口
        return 1;
    }

    // 创建socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        WSAGetLastError();
        WSACleanup();
        system("pause");
        return 1;
    }

    // 创建SOCKADDR结构，存储IPv4套接字的地址信息
    SOCKADDR_IN serverAddr;
    const char* SERVER_IP = getLocalIP();
    cout << "服务器端所在IP地址为：" << SERVER_IP << "，占用的端口号是：" << LOCAL_PORT << endl;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(LOCAL_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr.S_un.S_addr);

    // 将socket与网络地址信息绑定
    int addrlen = sizeof(SOCKADDR);
    bind(server_socket, (SOCKADDR*)&serverAddr, addrlen);

    cout << "服务器资源已就绪，Waiting for joining ..." << endl;
    // 循环监听，直至服务器退出
    while (QUIT == false)
    {
        listen(server_socket, MAX_QUEUE);
        int index = findInQueue();
        if (index != -1) {
            // accept 成功后，会自动填充 SOCKADDR 结构体
            client_sockets[index] = accept(server_socket, (SOCKADDR*)&clientAddrs[index], &addrlen);
            if (client_sockets[index] != INVALID_SOCKET) {
                CLIENT_NUMBER++;
                online[index] = true;

                //获取用户连接时间：
                time_t now = time(0);
                char* connTime = ctime(&now);
                cout << connTime <<"客户端新建连接成功，当前聊天室内共有：" << CLIENT_NUMBER << "人" << endl;

                memset(sendBuff_S, 0, BUFFMAX);
                memset(recvBuff_S, 0, BUFFMAX);
                recv(client_sockets[index], recvBuff_S, BUFFMAX, 0);
                // 接受消息，填充用户信息：
                if (recvBuff_S[0] == 'n') { //n消息头，代表传输的是用户名字
                    ClientInfo[index].id = index;
                    int t = 1;
                    while (recvBuff_S[t] != '\0') {
                        ClientInfo[index].name[t - 1] = recvBuff_S[t];
                        t = t + 1;
                    }
                    ClientInfo[index].name[t] = '\0';
                }
                strcat(sendBuff_S, connTime);
                strcat(sendBuff_S, "连接成功，欢迎来到聊天室！\n");
                strcat(sendBuff_S, "You can chat with all people with any limit,and you can also use some commands as below:\n");
                strcat(sendBuff_S, "-\t\\usrs\t获取当前在线用户名单\n");
                strcat(sendBuff_S, "-\t\\to usrname:\t和某个用户说悄悄话\n");
                strcat(sendBuff_S, "-\t\\quit\t退出聊天室");
                send(client_sockets[index], sendBuff_S, BUFFMAX, 0);
                //为每个客户端新建线程来接受消息
                client_threads[index] = CreateThread(NULL, NULL, recvMessage_S, &ClientInfo[index], 0, NULL);
            }
        }
    }
    WSACleanup();
    return 0;
}

char* getLocalIP()
{
    cout << "请输入你的当前IP： ";
    char* ip = new char[20];
    cin >> ip;
    return ip;
}

int findInQueue()
{
    for (int i = 0; i < MAX_QUEUE; i++) {
        if (online[i] == false) {
            return i;
        }
    }
    return -1;
}

DWORD WINAPI recvMessage_S(LPVOID lpParam)
{
    clientInfo* info = (clientInfo*)lpParam;
    int cid = info->id;
    memset(recvBuff_S, 0, BUFFMAX);
    while (true) {
        if (recv(client_sockets[cid], recvBuff_S, BUFFMAX, 0) >= 0) {
            // 在服务端显示用户发来的信息
            cout << recvBuff_S << endl;
            if (recvBuff_S[0] == '\\') { //首字符为反斜杠代表命令，由服务端返回一些信息
                // \usrs：返回在线用户列表给客户端
                if (recvBuff_S[1] == 'u' && recvBuff_S[2] == 's' && recvBuff_S[3] == 'r' && recvBuff_S[4] == 's') {
                    char tempBuff[BUFFMAX];
                    memset(tempBuff, 0, BUFFMAX);
                    strcat(tempBuff, "在线用户列表：\n");
                    for (int i = 0; i < MAX_QUEUE; i++) {
                        if (online[i] == true) {
                            const char* tempName = ClientInfo[i].name;
                            strcat(tempBuff, "-\t");
                            strcat(tempBuff, tempName);
                            strcat(tempBuff, "\n");
                        }
                    }
                    send(client_sockets[cid], tempBuff, BUFFMAX, 0);                  
                }
                else if (recvBuff_S[1] == 't' && recvBuff_S[2] == 'o') { //私聊命令
                    // 获取用户要私聊的对象
                    char tempName[20] = { 0 };
                    int k = 4;
                    while (recvBuff_S[k] != ':') {
                        tempName[k - 4] = recvBuff_S[k];
                        k++;
                    }
                    tempName[k] = '\0';

                    char tempBuff[BUFFMAX];
                    memset(tempBuff, 0, BUFFMAX);
                    memset(sendBuff_S, 0, BUFFMAX);

                    // 查询用户是否存在且在线
                    for (int i = 0; i < MAX_QUEUE; i++) {
                        if (strcmp(ClientInfo[i].name, tempName) == 0 && online[i] == true) {
                            strcat(sendBuff_S, "来自");
                            strcat(sendBuff_S, info->name);
                            strcat(sendBuff_S, "的悄悄话：");
                            strcat(tempBuff, recvBuff_S);
                            strcat(sendBuff_S, tempBuff + k + 1);
                            send(client_sockets[i], sendBuff_S, BUFFMAX, 0);
                            break;
                        }
                        if (i == MAX_QUEUE-1) {
                            // 如果不在线返回提示信息
                            memset(tempBuff, 0, BUFFMAX);
                            strcat(tempBuff, "您要私聊的对象不存在或已下线！\n");
                            send(client_sockets[cid], tempBuff, BUFFMAX, 0);
                        }
                    }
                }
                else if (recvBuff_S[1] == 'q' && recvBuff_S[2] == 'u' && recvBuff_S[3] == 'i' && recvBuff_S[4] == 't') {
                    // \quit 退出命令，改变online，关闭socket，告知其他用户下线
                    online[cid] = false;
                    CLIENT_NUMBER--;
                    cout << info->name << "下线了，当前聊天室内共有：" << CLIENT_NUMBER << "人" << endl;
                    memset(sendBuff_S, 0, BUFFMAX);
                    strcat(sendBuff_S, info->name);
                    strcat(sendBuff_S, "下线了");
                    for (int i = 0; i < MAX_QUEUE; i++) {
                        if (online[i] == true) {
                            send(client_sockets[i], sendBuff_S, BUFFMAX, 0);
                        }
                    }
                    closesocket(client_sockets[cid]);
                }
                continue;
            }
            // 发给所有在线的客户
            memset(sendBuff_S, 0, BUFFMAX);
            strcat(sendBuff_S, recvBuff_S);
            for (int i = 0; i < MAX_QUEUE; i++) {
                if (online[i] == true) {
                    send(client_sockets[i], sendBuff_S, BUFFMAX, 0);
                }
            }
        }
    }
    return 0;
}