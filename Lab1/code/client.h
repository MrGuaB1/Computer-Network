#pragma once
#include <winsock2.h>
#pragma comment(lib,"ws2_32.lib")
const int BUFFSIZE = 1024;
int client(); //�ͻ��˳���
char* getServerIP(); //��ȡ����˵�IP��ַ
DWORD WINAPI recvMessage_C(LPVOID lpParam); //�ͻ��˽�����Ϣ
