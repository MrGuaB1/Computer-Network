#pragma once
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")
const int BUFFMAX = 1024;
const int MAX_QUEUE = 5; //�����Ϊ5��
int server(); //�������˳���
char* getLocalIP(); //����ͨ��ipconfig��ȡ��IP��ַ
int findInQueue(); //�����Ƿ��п�λ��û���򷵻�-1
DWORD WINAPI recvMessage_S(LPVOID lpParam); //����˽�����Ϣ
struct clientInfo { //��¼�ͻ���Ϣ
    int id; // ���б��
    char name[20];
};
