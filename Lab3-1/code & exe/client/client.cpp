#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include "segment.h"
#pragma comment (lib, "ws2_32.lib")

// ȫ�ֱ������壺
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // ��ʼ�����к�
int addrLen = sizeof(clientAddr); // ����UDP::recvfrom

// ����������
bool shakeHands(); // UDPʵ����������
bool waveHands(); // UDPʵ���Ĵλ���
bool sendSegment(Message&); // �������ݶ�
bool sendFile(string);

using namespace std;
int main()
{
	// ��ʼ��winsock��
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (res != 0) {
		cout << "winsock���ʼ��ʧ�ܣ�";
		return 1;
	}

	// �½�socket��
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0); //���ݱ��׽��֣�UDPЭ��
	unsigned long on = 1;
	ioctlsocket(clientSocket, FIONBIO, &on);

	// ����ַ��Ϣ�ṹ�壺
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, LocalIP, &serverAddr.sin_addr.S_un.S_addr);

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, LocalIP, &clientAddr.sin_addr.S_un.S_addr);

	// �����׽���ѡ�������ȴ�ʱ��
	setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&MAX_WAIT_TIME, sizeof(MAX_WAIT_TIME));
	
	// ����˰���Ϣ��
	bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));

	// �������ֽ������ӣ�
	bool alive = shakeHands();
	cout << "�������ֽ������ӳɹ������ڿ���ѡ���ϴ��ļ���Ͽ�����" << endl;
	
	// ��ʼ�ļ�����

	int which;
	cout << "input 1-3 to transport 1-3.jpg or input 4 to transport helloworld.txt��";
	cin >> which;
	switch (which) 
	{
		case 1:sendFile("1.jpg"); break;
		case 2:sendFile("2.jpg"); break;
		case 3:sendFile("3.jpg"); break;
		case 4:sendFile("helloworld.txt"); break;
	}
	cout << "���˳����䣬׼���Ĵλ����ͷ����ӣ�" << endl;

	//�Ĵλ����ͷ����ӣ�
	waveHands();

	cout << "���ͷ����ӣ�" << endl;
	WSACleanup();
	system("pause");
	return 0;
}

bool shakeHands()
{
	Message message1, message2, message3;
	// ��һ�����֣��ͻ��������˷������󣬲���SYN��λ��
	message1.setPort(CLIENT_PORT, SERVER_PORT);
	message1.set_SYN();
	message1.seqNum = ++seq;
	message1.setCheck();
	int bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
	clock_t shake1 = clock();
	if (bytes == 0) {
		cout << "��һ������ʱ����Ϣ���ͷ�������" << endl;
		return false;
	}
	cout << "---------- �ͻ����ѷ��͵�һ�����ֵ���Ϣ���ȴ��ڶ������� ----------" << endl;

	// �ڶ������֣�����˷�����Ϣ���ͻ��ˣ�����SYN��ACK��λ��
	while (true) {
		int bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes == 0) {
			cout << "�ڶ�������ʱ����Ϣ���շ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// �ɹ����յ���Ϣ������������У��ͣ�SYN��ACK�ֶΣ��Լ�seq��
			if ((message2.flag && SYN) && (message2.flag && ACK) && message2.getCheck() && message2.ackNum == message1.seqNum) {
				cout << "---------- �ڶ������ֳɹ��� ----------" << endl;
				break;
			}
		}
		if (clock() - shake1 >= MAX_WAIT_TIME) {
			cout << "---------- ��һ�����ֳ�ʱ���������´��䣡 ----------" << endl;
			int bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
			shake1 = clock(); // ���¼�ʱ
		}
	}

	// ���������֣��ͻ��������˷���ֻ��ACK��λ��
	message3.setPort(CLIENT_PORT, SERVER_PORT);
	message3.set_ACK();
	message3.seqNum = ++seq;
	message3.setCheck();
	bytes = sendto(clientSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&serverAddr, addrLen);
	cout << "---------- �ͻ����ѷ��͵��������ֵ���Ϣ����׼������ ----------" << endl;
	return true;
}

bool waveHands()
{
	Message message1, message2, message3, message4;
	// ��һ�λ��֣��ͻ��������˷��𣬲���FIN��λ��
	message1.setPort(CLIENT_PORT, SERVER_PORT);
	message1.set_FIN();
	message1.seqNum = ++seq;
	message1.setCheck();
	int bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
	if (bytes == 0) {
		cout << "��һ�λ���ʱ����Ϣ���ͷ�������" << endl;
		return false;
	}
	clock_t wave1 = clock();
	cout << "---------- �ͻ����ѷ��͵�һ�λ��ֵ���Ϣ���ȴ��ڶ��λ��� ----------" << endl;

	// �ڶ��λ��֣��������ͻ��ˣ�ACK��λ��
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// �ɹ����յ���Ϣ������������У��ͣ�ACK�ֶΣ��Լ�seq��
			if (message2.getCheck() && (message2.flag && ACK) && message2.ackNum == message1.seqNum) {
				cout << "---------- �ڶ��λ��ֳɹ��� ----------" << endl;
				break;
			}
		}
		if (clock() - wave1 >= MAX_WAIT_TIME) {
			cout << "---------- ��һ�λ��ֳ�ʱ���������´��䣡 ----------" << endl;
			bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
			wave1 = clock();
		}
	}

	// �����λ��֣��������ͻ��˷����ͷ����ӱ��ģ�FIN��ACK��λ��
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			if (message3.getCheck() && (message3.flag && FIN) && (message3.flag && ACK)) {
				cout << "---------- �����λ��ֳɹ��� ----------" << endl;
				break;
			}
		}
	}

	// ���Ĵλ��֣��ͻ�������������ȷ�ϣ�����ACK��λ��
	message4.setPort(CLIENT_PORT, SERVER_PORT);
	message4.set_ACK();
	message4.ackNum = message3.seqNum;
	message4.setCheck();
	bytes = sendto(clientSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&serverAddr, addrLen);
	cout << "---------- �ͻ����ѷ��͵��Ĵλ��ֵ���Ϣ����Դ׼���رգ� ----------" << endl;

	// �ͻ��˻�����ȴ�2*MSL����ֹ���һ��ACK��δ���
	clock_t waitFor2MSL = clock();
	Message ackMessage;
	while (clock() - waitFor2MSL < 2 * MAX_WAIT_TIME) {
		int bytes = recvfrom(clientSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// �ط����һ��ACK�������¼�ʱ2*MSL
			sendto(clientSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&serverAddr, addrLen);
			waitFor2MSL = clock();
		}
	}
	cout << "---------- ��Դ�ͷ���ϣ����ӶϿ��� ----------" << endl;
}

bool sendSegment(Message& sendMessage)
{
	// ���ͱ��ĶΣ�����ʼ��ʱ
	sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
	clock_t send_time = clock();
	cout << "�ͻ��˷����� seq = " << sendMessage.seqNum << " �ı��Ķ�" << endl;
	Message recvMessage;
	while (true) {
		int bytes = recvfrom(clientSocket, (char*)&recvMessage, sizeof(recvMessage), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// �ɹ��յ���Ϣ�����ACK��seq�������������˻ظ�һ��ACK
			if ((recvMessage.flag && ACK) && (recvMessage.ackNum == sendMessage.seqNum)) {
				cout << "�ͻ����յ��˷���˷����� ack = " << recvMessage.ackNum << " ��ȷ�ϱ���" << endl;
				return true;
			}
		}
		if (clock() - send_time >= MAX_WAIT_TIME) {
			cout << "seq = " << sendMessage.seqNum << " �ı��Ķη��ͳ�ʱ���ͻ����������´��� ......" << endl;
			sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr,addrLen);
			send_time = clock();
		}
	}
	return false;
}

bool sendFile(string fileName)
{
	int startTime = clock();
	ifstream fin(fileName.c_str(), ifstream::binary); // ���ֽ����򿪴����ļ�
	BYTE* transFile = new BYTE[LONGEST];
	unsigned int fileSize = 0;
	BYTE currByte = fin.get();
	while (fin) {
		transFile[fileSize++] = currByte;
		currByte = fin.get();
	}
	fin.close();
	
	// �ȷ����ļ�������Ϣ��
	Message fileMessage;
	fileMessage.setPort(CLIENT_PORT, SERVER_PORT);
	fileMessage.size = fileSize;
	fileMessage.seqNum = ++seq;
	// ���ļ���������Ϣ��data�ֶΣ�
	for (int i = 0; i < fileName.size(); i++)//��䱨�����ݶ�
		fileMessage.data[i] = fileName[i];
	fileMessage.data[fileName.size()] = '\0';
	fileMessage.setCheck();
	if (sendSegment(fileMessage) == true) 
		cout << "�ͻ��˳ɹ�������Ϊ " << fileName << " ���ļ������С�ǣ�" << fileSize << " �ֽ�" << endl;
	else {
		cout << "�ļ� " << fileName << " ����ʧ�ܣ�" << endl;
		return false;
	}

	// �ٷ����ļ��������ݣ�
	// ����γ��ȣ�
	int segments = fileSize / MSS;
	int leftBytes = fileSize % MSS;
	for (int i = 0; i < segments; i++) {
		Message sendMessage;
		sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
		sendMessage.seqNum = ++seq;
		for (int j = 0; j < MSS; j++)
			sendMessage.data[j] = transFile[i * MSS + j];
		sendMessage.setCheck();
		if (sendSegment(sendMessage) == true) 
			cout << "�ͻ��˳ɹ�����seq=" << sendMessage.seqNum << "�ı��ĶΣ�" << endl;
		else {
			cout << " ��" << i << "������Ķη���ʧ�ܣ�" << endl;
			return false;
		}
	}
	if (leftBytes != 0) {
		Message sendMessage;
		sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
		sendMessage.seqNum = ++seq;
		for (int j = 0; j < leftBytes; j++)
			sendMessage.data[j] = transFile[segments * MSS + j];
		sendMessage.setCheck();
		if (sendSegment(sendMessage) == true)
			cout << "�ͻ��˳ɹ�����seq=" << sendMessage.seqNum << "�ı��ĶΣ�" << endl;
		else {
			cout << "���µı��Ķη���ʧ�ܣ�" << endl;
			return false;
		}
	}
	clock_t endTime = clock();
	cout << "�ļ�" << fileName << "���ܴ���ʱ��Ϊ��" << (endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
	cout << "������̵�������Ϊ:" << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << "byte/s" << endl << endl;
	return true;
}
