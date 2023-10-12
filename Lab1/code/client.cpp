#include "client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#pragma warning(disable:4996)
using namespace std;

int CLIENT_PORT = 11451; //要实现群聊效果，需要所有client绑定到一个端口上
int SERVER_PORT = 19191;
const char* LOCAL_IP = "127.0.0.1";
char recvBuff_C[BUFFSIZE]; //客户端接受消息缓冲区
char sendBuff_C[BUFFSIZE]; //客户端发送消息缓冲区
SOCKET client_socket;

int client()
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
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        WSAGetLastError();
        WSACleanup();
        system("pause");
        return 1;
    }

    // 创建SOCKADDR结构，存储IPv4套接字的地址信息
    SOCKADDR_IN clientAddr, serverAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(CLIENT_PORT); // 转换为网络所需的大端序
    // 将字符串表示的IPv4地址转换为二进制格式，并将结果存储在 sin_addr 中，S_un.S_addr 是一个32位整数
    inet_pton(AF_INET, LOCAL_IP, &clientAddr.sin_addr.S_un.S_addr);

    // 获取并绑定服务器信息
    const char* SERVER_IP = getServerIP();
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr.S_un.S_addr);

    string usrname; //用户昵称
    cout << "请输入一个用于聊天的昵称： ";
    cin >> usrname;
    cin.get();
    usrname = "n" + usrname;

    // 连接远程服务器
    if (connect(client_socket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) != SOCKET_ERROR) {

        //发送自己的信息，填充服务器的用户结构
        int length = usrname.length() + 2;
        const char* sendName = usrname.c_str();
        memset(sendBuff_C, 0, BUFFSIZE);
        strcat(sendBuff_C, sendName);
        send(client_socket, sendName, length, 0);

        //创建线程，用于接受消息：
        HANDLE recv_thread = CreateThread(NULL, NULL, recvMessage_C, NULL, 0, NULL);

        //主线程用于发送消息
        while (true) {
            memset(sendBuff_C, 0, BUFFSIZE);
            char input[BUFFSIZE];
            cin.getline(input, BUFFSIZE);
            if (input[0] == '\\') { // 如果是命令消息那么直接发出，不进行修饰
                strcat(sendBuff_C, input);
                send(client_socket, sendBuff_C, BUFFSIZE, 0);
                continue;
            }
            //获取当前时间并格式化：
            time_t currentTime = time(0);
            struct tm now;
            localtime_s(&now, &currentTime);
            stringstream formattedTime;
            formattedTime << setfill('0') << setw(2) << now.tm_hour << ":"
                << setfill('0') << setw(2) << now.tm_min << ":"
                << setfill('0') << setw(2) << now.tm_sec;
            string timeString = usrname.substr(1) + " " + formattedTime.str() + "：";
            const char* mesg = timeString.c_str();
            strcat(sendBuff_C, mesg);
            strcat(sendBuff_C, input);
            send(client_socket, sendBuff_C, BUFFSIZE, 0);
        }
    }
    else {
        cout << "当前服务器繁忙，请稍后再试..." << endl;
    }
    

    WSACleanup();
    return 0;
}

char* getServerIP()
{
    char* ip = new char[20];
    cout << "请输入你要连接的服务器所在IP地址： ";
    cin >> ip;
    return ip;
}

DWORD WINAPI recvMessage_C(LPVOID lpParam)
{
    while (true) {
        // 清空缓冲区
        memset(recvBuff_C, 0, BUFFSIZE);
        recv(client_socket, recvBuff_C, BUFFSIZE, 0);
        cout << recvBuff_C << endl;
    }
    return 0;
}