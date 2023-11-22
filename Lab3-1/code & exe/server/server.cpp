#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <sstream>
#include "segment.h"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

// ȫ�ֱ������壺
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // ��ʼ�����к�
int addrLen = sizeof(serverAddr); // ����UDP::recvfrom

// ����������
bool shakeHands(); // UDPʵ����������
bool waveHands(); // UDPʵ���Ĵλ���
bool recvSegment(Message&); // �������ݶ�

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
	serverSocket = socket(AF_INET, SOCK_DGRAM, 0); //���ݱ��׽��֣�UDPЭ��

	// ����ַ��Ϣ�ṹ�壺
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, LocalIP, &serverAddr.sin_addr.S_un.S_addr);

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, LocalIP, &clientAddr.sin_addr.S_un.S_addr);

	// �����׽���ѡ�������ȴ�ʱ��
	setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&MAX_WAIT_TIME, sizeof(MAX_WAIT_TIME));

	// ����˰�socket��addr��
	res = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	if (res != 0) {
		cout << "�����socket���ַ��Ϣ��ʧ�ܣ�" << endl;
		WSACleanup();
		return 1;
	}
	cout << "��������Դ�Ѿ�����waiting for connecting ..." << endl;
	
	// �������ֽ�������
	bool alive = shakeHands();
	
	// ��ʼ�ļ����䣬���Ƚ����ļ���������Ϣ��
	Message fileMessage; // �ļ���Ϣ����
	unsigned int fileSize; // ��¼�ļ���С
	char fileName[100] = { 0 };
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&fileMessage, sizeof(fileMessage), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes > 0) {
			// �ɹ��յ���Ϣ�����ACK��seq��Ȼ����ͻ��˻ظ�һ��ACK��
			if (fileMessage.getCheck() && fileMessage.seqNum == seq + 1) {
				seq++;
				fileSize = fileMessage.size;
				for (int i = 0; fileMessage.data[i]; i++)
					fileName[i] = fileMessage.data[i];
				cout << "����˼��������ļ���" << fileName << "�����СΪ��" << fileSize << endl;
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK(); //ACK��λ��ʾ��Ӧ
				ackMessage.ackNum = fileMessage.seqNum; // �ظ�����Ӧ�ž��ǽ��յ����к�
				ackMessage.setCheck(); // ����У���
				// ����ACK����
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
				cout << "�������յ� seq = " << fileMessage.seqNum << "�ı��ĶΣ������� ack = " << ackMessage.ackNum << " �Ļظ����Ķ�" << endl;
				break;
			}
		}
		else if(fileMessage.getCheck() && fileMessage.seqNum != seq + 1){
			Message ackMessage;
			ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
			ackMessage.set_ACK(); //ACK��λ��ʾ��Ӧ
			ackMessage.ackNum = fileMessage.seqNum; // �ظ�����Ӧ�ž��ǽ��յ����к�
			ackMessage.setCheck(); // ����У���
			sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
			cout << "server�յ����ظ��ı��ĶΡ� seq = " << fileMessage.seqNum << endl;
		}
	}

	// ��ʼ�����ļ����ݣ�����recvSegment()������
	// ����γ��ȣ�
	int segments = fileSize / MSS;
	int leftBytes = fileSize % MSS;
	char* transFile = new char[fileSize];
	for (int i = 0; i < segments; i++) {
		Message fileData;
		if (recvSegment(fileData) == true)
			cout << "���ݱ�" << fileData.seqNum << "���ճɹ���" << endl;
		for (int j = 0; j < MSS; j++)
			transFile[i * MSS + j] = fileData.data[j];
	}
	if (leftBytes != 0) {
		Message fileData;
		if (recvSegment(fileData) == true)
			cout << "���ݱ�" << fileData.seqNum << "���ճɹ���" << endl;
		for (int j = 0; j < leftBytes; j++)
			transFile[segments * MSS + j] = fileData.data[j];
	}

	cout << "ȫ�����ݱ�������ϣ�����д���ļ���" << endl;
	FILE* newFile;
	newFile = fopen(fileName, "wb");
	fwrite(transFile, fileSize, 1, newFile);
	fclose(newFile);
	cout << "д���ļ���ϣ�" << endl;

	// �Ĵλ����ͷ����ӣ�
	waveHands();
	cout << "�����ѳɹ��ͷţ�" << endl;

	WSACleanup();
	system("pause");
	return 0;
}
bool shakeHands()
{
	// �������ֹ����ֵı��Ļ�����
	Message message1, message2, message3;
	while (true) {
		// ��һ�����֣��ͻ������������֣�����SYN��λ
		int bytes = recvfrom(serverSocket, (char*)&message1, sizeof(message1), 0, (SOCKADDR*)&clientAddr, &addrLen);
		if (bytes == 0) {
			cout << "��һ������ʱ����Ϣ���շ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// �ɹ����յ���Ϣ������������У��ͣ�SYN�ֶΣ��Լ�seq��
			// TCPЭ���У��涨SYN���Ķβ���Я�����ݣ�����Ҫ����һ�����кţ����seq�����������Ҫ+1
			if (!(message1.flag && SYN) || !(message1.getCheck()) || !(message1.seqNum == seq + 1)) {
				cout << "��һ������ʱ��SYN�λ�У��Ͷλ����кŷ�������" << endl;
				return false;
			}
			// ��һ�����ֳɹ�
			cout << "---------- ��һ�����ֳɹ��� ----------" << endl;

			// �ڶ������֣�����˷�����Ϣ���ͻ��ˣ����к�+1��SYN��ACK��λ
			seq++;
			message2.setPort(SERVER_PORT, CLIENT_PORT); // �󶨶˿���Ϣ
			// ack�ž��Ƿ�����seq��
			message2.ackNum = message1.seqNum;
			// ��SYN��ACK��λ��Ȼ���ͻظ���Ϣ
			message2.set_ACK();
			message2.set_SYN();
			message2.setCheck(); // ����У���
			bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (SOCKADDR*)&clientAddr, addrLen);
			if (bytes == 0) {
				cout << "�ڶ�������ʱ����Ϣ���ͷ�������" << endl;
				return false;
			}
			clock_t shake2start = clock(); // �ڶ������ֵ���Ϣ�����ʱ��Ҫ�ش�
			cout << "---------- �������ѷ��͵ڶ������ֵ���Ϣ���ȴ����������� ----------" << endl;

			// ���������֣��ͻ��˷�����Ϣ������ˣ�SYN������λ��ACK��Ҫ��λ��
			while (true) {
				bytes = recvfrom(serverSocket, (char*)&message3, sizeof(message3), 0, (SOCKADDR*)&clientAddr, &addrLen);
				if (bytes == 0) {
					cout << "����������ʱ����Ϣ���շ�������" << endl;
					return false;
				}
				else if (bytes > 0) {
					// �ɹ����յ���Ϣ������������У��ͣ�SYN�ֶΣ��Լ�seq��
					if (message3.getCheck() && (message3.flag && ACK) && (message3.seqNum == seq + 1)) {
						seq++;
						cout << "---------- ���������ֳɹ��� ----------" << endl;
						return true;
					}
				}
				// ���յ�����Ϣ���ԣ������ǲ��ǵڶ������ֵı���û���յ���
				if (clock() - shake2start >= MAX_WAIT_TIME) {
					cout << "---------- �ڶ������ֳ�ʱ���������´��䣡 ----------" << endl;
					bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (SOCKADDR*)&clientAddr, addrLen);
					if (bytes == 0) {
						cout << "�ڶ�������ʱ����Ϣ���ͷ�������" << endl;
						return false;
					}
					shake2start = clock(); // ���¼�ʱ
				}				
			}
		}
	}
	return false;
}
bool waveHands()
{
	Message message1, message2, message3, message4;
	while (true) {
		// ��һ�λ��֣��ͻ��˷����ͷ���������FIN��λ��
		int bytes = recvfrom(serverSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes == 0) {
			cout << "��һ�λ���ʱ����Ϣ���շ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// ��Ϣ���ճɹ�������������У��ͣ�FIN�ֶΣ��Լ�seq��
			if (!(message1.flag && FIN) || !message1.getCheck() || !(message1.seqNum == seq + 1)) {
				cout << "��һ�λ���ʱ��FIN�λ�У��Ͷλ����кŷ�������" << endl;
				return false;
			}
			// ��һ�λ��ֳɹ�
			cout << "---------- ��һ�λ��ֳɹ��� ----------" << endl;

			// �ڶ��λ��֣��������ͻ��˻�Ӧ��ֻ���ACK��λ��
			message2.setPort(SERVER_PORT, CLIENT_PORT);
			message2.ackNum = message1.seqNum;
			message2.set_ACK();
			message2.setCheck(); // ����У���
			bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&clientAddr, addrLen);
			if (bytes == 0) {
				cout << "�ڶ��λ���ʱ����Ϣ���ͷ�������" << endl;
				return false;
			}
			clock_t shake2start = clock();
			cout << "---------- �������ѷ��͵ڶ��λ��ֵ���Ϣ�� ----------" << endl;
			break;
		}
	}

	// �����λ��֣�����˷����ͷű��ģ���FIN��ACK��λ�����ȴ��ͻ��˷�����ȷ�ϱ���
	message3.setPort(SERVER_PORT, CLIENT_PORT);
	message3.set_ACK();
	message3.set_FIN();
	message3.seqNum = seq++;
	message3.setCheck(); // ����У���
	int bytes = sendto(serverSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&clientAddr, addrLen);
	clock_t shake3start = clock();
	if (bytes == 0) {
		cout << "�����λ���ʱ����Ϣ���ͷ�������" << endl;
		return false;
	}
	cout << "---------- �������ѷ��͵����λ��ֵ���Ϣ�� ----------" << endl;

	// ���Ĵλ��֣��ͻ������������ȷ�ϣ�
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes == 0) {
			cout << "���Ĵλ���ʱ����Ϣ���ͷ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// ��Ϣ���ճɹ�������������У��ͣ�ACK�ֶΣ��Լ�seq��
			if ((message4.flag && ACK) && message4.getCheck() && (message4.ackNum == message3.seqNum)) {
				cout << "---------- ���Ĵλ��ֳɹ��� ----------" << endl;
				return true;
			}
		}
		if (clock() - shake3start >= MAX_WAIT_TIME) {
			cout << "---------- �����λ��ֳ�ʱ���������´��䣡 ----------" << endl;
			bytes = sendto(serverSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&clientAddr, addrLen);
			shake3start = clock();
		}
	}
	return false;
}
bool recvSegment(Message& recvMessage) {
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&recvMessage, sizeof(recvMessage), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes > 0) {
			// �ɹ��յ���Ϣ�����ACK��seq��Ȼ����ͻ��˻ظ�һ��ACK��
			if (recvMessage.getCheck() && (recvMessage.seqNum == seq + 1)) {
				seq++;
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK(); //ACK��λ��ʾ��Ӧ
				ackMessage.ackNum = recvMessage.seqNum; // �ظ�����Ӧ�ž��ǽ��յ����к�
				ackMessage.setCheck(); // ����У���
				// ����ACK����
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
				cout << "�������յ� seq = " << recvMessage.seqNum << "�ı��ĶΣ������� ack = " << ackMessage.ackNum << " �Ļظ����Ķ�" << endl;
				return true;
			}
			// ���У�����ȷ�����յ������кŲ���Ԥ�����кţ�����������ͣ�Ȼ���
			// �������һ�����յ����ظ��ı��ĶΣ������ڵȴ�ȱʧ�ı��Ķ�
			else if (recvMessage.getCheck() && (recvMessage.seqNum != seq + 1)) {
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK(); //ACK��λ��ʾ��Ӧ
				ackMessage.ackNum = recvMessage.seqNum; // �ظ�����Ӧ�ž��ǽ��յ����к�
				ackMessage.setCheck(); // ����У���
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr,addrLen);
				cout << "server�յ����ظ��ı��ĶΡ� seq = " << recvMessage.seqNum << endl;
			}
		}
		else if (bytes == 0)
			return false;
	}
}