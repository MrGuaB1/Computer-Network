#include "client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>
#pragma warning(disable:4996)
using namespace std;

int CLIENT_PORT = 11451; //Ҫʵ��Ⱥ��Ч������Ҫ����client�󶨵�һ���˿���
int SERVER_PORT = 19191;
const char* LOCAL_IP = "127.0.0.1";
char recvBuff_C[BUFFSIZE]; //�ͻ��˽�����Ϣ������
char sendBuff_C[BUFFSIZE]; //�ͻ��˷�����Ϣ������
SOCKET client_socket;

int client()
{
    // ��ʼ�� winsock��
    WSADATA wsadata;
    int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
    if (res != 0) {
        WSAGetLastError();
        system("pause"); //���������رմ���
        return 1;
    }

    // ����socket
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        WSAGetLastError();
        WSACleanup();
        system("pause");
        return 1;
    }

    // ����SOCKADDR�ṹ���洢IPv4�׽��ֵĵ�ַ��Ϣ
    SOCKADDR_IN clientAddr, serverAddr;
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(CLIENT_PORT); // ת��Ϊ��������Ĵ����
    // ���ַ�����ʾ��IPv4��ַת��Ϊ�����Ƹ�ʽ����������洢�� sin_addr �У�S_un.S_addr ��һ��32λ����
    inet_pton(AF_INET, LOCAL_IP, &clientAddr.sin_addr.S_un.S_addr);

    // ��ȡ���󶨷�������Ϣ
    const char* SERVER_IP = getServerIP();
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr.S_un.S_addr);

    string usrname; //�û��ǳ�
    cout << "������һ������������ǳƣ� ";
    cin >> usrname;
    cin.get();
    usrname = "n" + usrname;

    // ����Զ�̷�����
    if (connect(client_socket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR)) != SOCKET_ERROR) {

        //�����Լ�����Ϣ�������������û��ṹ
        int length = usrname.length() + 2;
        const char* sendName = usrname.c_str();
        memset(sendBuff_C, 0, BUFFSIZE);
        strcat(sendBuff_C, sendName);
        send(client_socket, sendName, length, 0);

        //�����̣߳����ڽ�����Ϣ��
        HANDLE recv_thread = CreateThread(NULL, NULL, recvMessage_C, NULL, 0, NULL);

        //���߳����ڷ�����Ϣ
        while (true) {
            memset(sendBuff_C, 0, BUFFSIZE);
            char input[BUFFSIZE];
            cin.getline(input, BUFFSIZE);
            if (input[0] == '\\') { // �����������Ϣ��ôֱ�ӷ���������������
                strcat(sendBuff_C, input);
                send(client_socket, sendBuff_C, BUFFSIZE, 0);
                continue;
            }
            //��ȡ��ǰʱ�䲢��ʽ����
            time_t currentTime = time(0);
            struct tm now;
            localtime_s(&now, &currentTime);
            stringstream formattedTime;
            formattedTime << setfill('0') << setw(2) << now.tm_hour << ":"
                << setfill('0') << setw(2) << now.tm_min << ":"
                << setfill('0') << setw(2) << now.tm_sec;
            string timeString = usrname.substr(1) + " " + formattedTime.str() + "��";
            const char* mesg = timeString.c_str();
            strcat(sendBuff_C, mesg);
            strcat(sendBuff_C, input);
            send(client_socket, sendBuff_C, BUFFSIZE, 0);
        }
    }
    else {
        cout << "��ǰ��������æ�����Ժ�����..." << endl;
    }
    

    WSACleanup();
    return 0;
}

char* getServerIP()
{
    char* ip = new char[20];
    cout << "��������Ҫ���ӵķ���������IP��ַ�� ";
    cin >> ip;
    return ip;
}

DWORD WINAPI recvMessage_C(LPVOID lpParam)
{
    while (true) {
        // ��ջ�����
        memset(recvBuff_C, 0, BUFFSIZE);
        recv(client_socket, recvBuff_C, BUFFSIZE, 0);
        cout << recvBuff_C << endl;
    }
    return 0;
}