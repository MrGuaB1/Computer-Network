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
bool QUIT = false; // �������Ƿ��˳�
int CLIENT_NUMBER = 0; //�ж�������ʹ�÷�����
bool online[MAX_QUEUE] = { false }; //���������Чλ
HANDLE client_threads[MAX_QUEUE];
clientInfo ClientInfo[MAX_QUEUE];
char recvBuff_S[BUFFMAX];
char sendBuff_S[BUFFMAX];
// �����û�����
SOCKET client_sockets[MAX_QUEUE];
SOCKADDR_IN clientAddrs[MAX_QUEUE];

int server()
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
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        WSAGetLastError();
        WSACleanup();
        system("pause");
        return 1;
    }

    // ����SOCKADDR�ṹ���洢IPv4�׽��ֵĵ�ַ��Ϣ
    SOCKADDR_IN serverAddr;
    const char* SERVER_IP = getLocalIP();
    cout << "������������IP��ַΪ��" << SERVER_IP << "��ռ�õĶ˿ں��ǣ�" << LOCAL_PORT << endl;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(LOCAL_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr.S_un.S_addr);

    // ��socket�������ַ��Ϣ��
    int addrlen = sizeof(SOCKADDR);
    bind(server_socket, (SOCKADDR*)&serverAddr, addrlen);

    cout << "��������Դ�Ѿ�����Waiting for joining ..." << endl;
    // ѭ��������ֱ���������˳�
    while (QUIT == false)
    {
        listen(server_socket, MAX_QUEUE);
        int index = findInQueue();
        if (index != -1) {
            // accept �ɹ��󣬻��Զ���� SOCKADDR �ṹ��
            client_sockets[index] = accept(server_socket, (SOCKADDR*)&clientAddrs[index], &addrlen);
            if (client_sockets[index] != INVALID_SOCKET) {
                CLIENT_NUMBER++;
                online[index] = true;

                //��ȡ�û�����ʱ�䣺
                time_t now = time(0);
                char* connTime = ctime(&now);
                cout << connTime <<"�ͻ����½����ӳɹ�����ǰ�������ڹ��У�" << CLIENT_NUMBER << "��" << endl;

                memset(sendBuff_S, 0, BUFFMAX);
                memset(recvBuff_S, 0, BUFFMAX);
                recv(client_sockets[index], recvBuff_S, BUFFMAX, 0);
                // ������Ϣ������û���Ϣ��
                if (recvBuff_S[0] == 'n') { //n��Ϣͷ������������û�����
                    ClientInfo[index].id = index;
                    int t = 1;
                    while (recvBuff_S[t] != '\0') {
                        ClientInfo[index].name[t - 1] = recvBuff_S[t];
                        t = t + 1;
                    }
                    ClientInfo[index].name[t] = '\0';
                }
                strcat(sendBuff_S, connTime);
                strcat(sendBuff_S, "���ӳɹ�����ӭ���������ң�\n");
                strcat(sendBuff_S, "You can chat with all people with any limit,and you can also use some commands as below:\n");
                strcat(sendBuff_S, "-\t\\usrs\t��ȡ��ǰ�����û�����\n");
                strcat(sendBuff_S, "-\t\\to usrname:\t��ĳ���û�˵���Ļ�\n");
                strcat(sendBuff_S, "-\t\\quit\t�˳�������");
                send(client_sockets[index], sendBuff_S, BUFFMAX, 0);
                //Ϊÿ���ͻ����½��߳���������Ϣ
                client_threads[index] = CreateThread(NULL, NULL, recvMessage_S, &ClientInfo[index], 0, NULL);
            }
        }
    }
    WSACleanup();
    return 0;
}

char* getLocalIP()
{
    cout << "��������ĵ�ǰIP�� ";
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
            // �ڷ������ʾ�û���������Ϣ
            cout << recvBuff_S << endl;
            if (recvBuff_S[0] == '\\') { //���ַ�Ϊ��б�ܴ�������ɷ���˷���һЩ��Ϣ
                // \usrs�����������û��б���ͻ���
                if (recvBuff_S[1] == 'u' && recvBuff_S[2] == 's' && recvBuff_S[3] == 'r' && recvBuff_S[4] == 's') {
                    char tempBuff[BUFFMAX];
                    memset(tempBuff, 0, BUFFMAX);
                    strcat(tempBuff, "�����û��б�\n");
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
                else if (recvBuff_S[1] == 't' && recvBuff_S[2] == 'o') { //˽������
                    // ��ȡ�û�Ҫ˽�ĵĶ���
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

                    // ��ѯ�û��Ƿ����������
                    for (int i = 0; i < MAX_QUEUE; i++) {
                        if (strcmp(ClientInfo[i].name, tempName) == 0 && online[i] == true) {
                            strcat(sendBuff_S, "����");
                            strcat(sendBuff_S, info->name);
                            strcat(sendBuff_S, "�����Ļ���");
                            strcat(tempBuff, recvBuff_S);
                            strcat(sendBuff_S, tempBuff + k + 1);
                            send(client_sockets[i], sendBuff_S, BUFFMAX, 0);
                            break;
                        }
                        if (i == MAX_QUEUE-1) {
                            // ��������߷�����ʾ��Ϣ
                            memset(tempBuff, 0, BUFFMAX);
                            strcat(tempBuff, "��Ҫ˽�ĵĶ��󲻴��ڻ������ߣ�\n");
                            send(client_sockets[cid], tempBuff, BUFFMAX, 0);
                        }
                    }
                }
                else if (recvBuff_S[1] == 'q' && recvBuff_S[2] == 'u' && recvBuff_S[3] == 'i' && recvBuff_S[4] == 't') {
                    // \quit �˳�����ı�online���ر�socket����֪�����û�����
                    online[cid] = false;
                    CLIENT_NUMBER--;
                    cout << info->name << "�����ˣ���ǰ�������ڹ��У�" << CLIENT_NUMBER << "��" << endl;
                    memset(sendBuff_S, 0, BUFFMAX);
                    strcat(sendBuff_S, info->name);
                    strcat(sendBuff_S, "������");
                    for (int i = 0; i < MAX_QUEUE; i++) {
                        if (online[i] == true) {
                            send(client_sockets[i], sendBuff_S, BUFFMAX, 0);
                        }
                    }
                    closesocket(client_sockets[cid]);
                }
                continue;
            }
            // �����������ߵĿͻ�
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