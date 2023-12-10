#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <sstream>
#include "segment.h"
#pragma comment(lib, "ws2_32.lib")

// ȫ�ֱ������壺
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // ��ʼ�����к�
int addrLen = sizeof(serverAddr); // ����UDP::recvfrom

// ����������
bool shakeHands(); // UDPʵ����������
bool waveHands(); // UDPʵ���Ĵλ���

// lab3-3������
int WINDOW_SIZE = 10;
int recv_flag = -1; // ������������յı��Ķα��
int left_space = WINDOW_SIZE; // ʣ��ɻ���ı��Ķ�
bool isRecved[LONGEST / MSS];
int sum = 0; // �ܵı��Ķ�����
int getAvailable(int start); // ��ȡ��start��ʼ�Ĵ����л��м����ɻ��汨�Ķ�

using namespace std;

int main()
{
	// ��ʼ��winsock��
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (res != 0) {
		std::cout << "winsock���ʼ��ʧ�ܣ�";
		return 1;
	}

	// �½�socket��
	serverSocket = socket(AF_INET, SOCK_DGRAM, 0); //���ݱ��׽��֣�UDPЭ��
	unsigned long on = 1;
	ioctlsocket(serverSocket, FIONBIO, &on);

	// ����ַ��Ϣ�ṹ�壺
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, LocalIP, &serverAddr.sin_addr.S_un.S_addr);

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, LocalIP, &clientAddr.sin_addr.S_un.S_addr);

	// �����׽���ѡ�������ȴ�ʱ��
	int timeout = 400;
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	// ����˰�socket��addr��
	res = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	if (res != 0) {
		std::cout << "�����socket���ַ��Ϣ��ʧ�ܣ�" << endl;
		WSACleanup();
		return 1;
	}
	std::cout << "��������Դ�Ѿ�����waiting for connecting ..." << endl;
	int size;
	std::cout << "input the window size you hope��";
	cin >> size;
	WINDOW_SIZE = size;
	
	// �������ֽ�������
	bool alive = shakeHands();

	// �ڴ������кţ�
	int next = 0;
	
	// ��ʼ�ļ����䣬���Ƚ����ļ���������Ϣ��
	Message fileMessage; // �ļ���Ϣ����
	unsigned int fileSize; // ��¼�ļ���С
	char fileName[100] = { 0 };
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&fileMessage, sizeof(fileMessage), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes > 0) {
			// �ɹ��յ���Ϣ�����ACK��seq��Ȼ����ͻ��˻ظ�һ��ACK��
			if (fileMessage.getCheck() && fileMessage.seqNum == next) {
				next++;
				fileSize = fileMessage.size;
				for (int i = 0; fileMessage.data[i]; i++)
					fileName[i] = fileMessage.data[i];
				std::cout << "����˼��������ļ���" << fileName << "�����СΪ��" << fileSize << endl;
				recv_flag++;
				isRecved[fileMessage.seqNum] = true;
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK(); //ACK��λ��ʾ��Ӧ
				ackMessage.ackNum = fileMessage.seqNum; // �ظ�����Ӧ�ž��ǽ��յ����к�
				ackMessage.setCheck(); // ����У���
				ackMessage.set_size(WINDOW_SIZE); // ���ý��մ���ͨ��
				// ����ACK����
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
				std::cout << "�������յ� seq = " << fileMessage.seqNum << "�ġ�˳�򡿱��ĶΣ������� ack = " << ackMessage.ackNum << " �Ļظ����Ķ�" << endl;
				break;
			}
		}
	}

	// ����γ��ȣ�
	int segments = fileSize / MSS;
	int leftBytes = fileSize % MSS;
	sum = leftBytes > 0 ? segments : segments + 1;
	char* transFile = new char[fileSize];
	int count = 0;
	while (true) {
		Message recvMessage;
		int bytes = recvfrom(serverSocket, (char*)&recvMessage, sizeof(recvMessage), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes > 0) {
			if (recvMessage.getCheck()) { 
				std::cout << "��ǰ��recv_flag:" << recv_flag << endl;
				// ����յ��ı��Ķ�������ģ���ô����飬�ҵ���һ��false�����ҵ��µĴ��ڿ�ʼλ��
				if (recvMessage.seqNum == recv_flag + 1) {
					isRecved[recvMessage.seqNum] = true;
					for (int j = recv_flag; j < segments+1; j++) {
						if (isRecved[j] == false) {
							recv_flag = j - 1;
							break;
						}
					}
					// ��ͻ��˷���˳�������Ϣ
					Message ackMessage;
					ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
					ackMessage.set_ACK(); 
					// �ظ�����Ӧ�ž��ǽ��յ����кţ���ʾ�ڴ����Ͷ˵� seqNum+1 �ı��Ķ�
					ackMessage.ackNum = recvMessage.seqNum;
					ackMessage.setCheck(); // ����У���
					ackMessage.set_base(recv_flag + 1);
					left_space = getAvailable(recv_flag + 1);
					// ���ڴ�Сͨ��
					ackMessage.set_size(left_space);
					// ����ACK����
					sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
					std::cout << "�������յ� seq = " << recvMessage.seqNum << "�ġ�˳�򡿱��ĶΣ������� ack = " << ackMessage.ackNum << " �Ļظ����Ķ�" << endl;
					std::cout << "������������Ϣ��ӡ��������˻������ڵĿ�ʼλ�ã�" << recv_flag + 1 << "����໹���Ի���" << left_space << "��ʧ���Ķ�" << endl;
					for (int j = 0; j < MSS; j++) 
						transFile[(recvMessage.seqNum - 1) * MSS + j] = recvMessage.data[j];
				}
				// ����յ�������ı��ĶΣ���ôҪ����һ���ɻ������������Ҵ��ڲ��ܷ����ƶ�
				else {
					// �Է����ı��Ķν��л���
					isRecved[recvMessage.seqNum] = true;
					// ��ͻ��˷������������Ϣ
					Message ackMessage;
					ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
					ackMessage.set_ACK();
					ackMessage.ackNum = recvMessage.seqNum;
					ackMessage.setCheck(); // ����У���
					// ���ڴ�Сͨ��
					left_space = getAvailable(recv_flag+1);
					ackMessage.set_base(recv_flag + 1);
					ackMessage.set_size(left_space);
					// ����ACK����
					sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
					std::cout << "�������������ˡ� seq = " << recvMessage.seqNum << "�ġ����򡿱��ĶΣ������� ack = " << ackMessage.ackNum << " �Ļظ����Ķ�" << endl;
					std::cout << "������������Ϣ��ӡ��������˻������ڵĿ�ʼλ�ã�" << recv_flag + 1 << "����໹���Ի���" << left_space << "��ʧ���Ķ�" << endl;
					for (int j = 0; j < MSS; j++) 
						transFile[(recvMessage.seqNum - 1) * MSS + j] = recvMessage.data[j];
				}				
				count++;
				if (count == segments) break;
			}
		}
	}
	if (leftBytes != 0) {
		while (true) {
			Message recvMessage;
			int bytes = recvfrom(serverSocket, (char*)&recvMessage, sizeof(recvMessage), 0, (sockaddr*)&clientAddr, &addrLen);
			if (bytes > 0) {
				std::cout << "���ݱ�" << recvMessage.seqNum << "���ճɹ���" << endl;
				isRecved[recvMessage.seqNum] = true;
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK();
				ackMessage.ackNum = recvMessage.seqNum;
				ackMessage.setCheck();
				// ���ڴ�Сͨ��
				ackMessage.set_size(left_space);
				// ����ACK����
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
				for (int j = 0; j < leftBytes; j++)
					transFile[segments * MSS + j] = recvMessage.data[j];
				break;
			}
		}
	}

	std::cout << "ȫ�����ݱ�������ϣ�����д���ļ���" << endl;
	FILE* newFile;
	newFile = fopen(fileName, "wb");
	std::fwrite(transFile, fileSize, 1, newFile);
	std::fclose(newFile);
	std::cout << "д���ļ���ϣ�" << endl;

	// �Ĵλ����ͷ����ӣ�
	waveHands();
	std::cout << "�����ѳɹ��ͷţ�" << endl;

	WSACleanup();
	std::system("pause");
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
			std::cout << "��һ������ʱ����Ϣ���շ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// �ɹ����յ���Ϣ������������У��ͣ�SYN�ֶΣ��Լ�seq��
			// TCPЭ���У��涨SYN���Ķβ���Я�����ݣ�����Ҫ����һ�����кţ����seq�����������Ҫ+1
			if (!(message1.flag && SYN) || !(message1.getCheck()) || !(message1.seqNum == seq + 1)) {
				std::cout << "��һ������ʱ��SYN�λ�У��Ͷλ����кŷ�������" << endl;
				return false;
			}
			// ��һ�����ֳɹ�
			std::cout << "---------- ��һ�����ֳɹ��� ----------" << endl;

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
				std::cout << "�ڶ�������ʱ����Ϣ���ͷ�������" << endl;
				return false;
			}
			clock_t shake2start = clock(); // �ڶ������ֵ���Ϣ�����ʱ��Ҫ�ش�
			std::cout << "---------- �������ѷ��͵ڶ������ֵ���Ϣ���ȴ����������� ----------" << endl;

			// ���������֣��ͻ��˷�����Ϣ������ˣ�SYN������λ��ACK��Ҫ��λ��
			while (true) {
				bytes = recvfrom(serverSocket, (char*)&message3, sizeof(message3), 0, (SOCKADDR*)&clientAddr, &addrLen);
				if (bytes == 0) {
					std::cout << "����������ʱ����Ϣ���շ�������" << endl;
					return false;
				}
				else if (bytes > 0) {
					// �ɹ����յ���Ϣ������������У��ͣ�SYN�ֶΣ��Լ�seq��
					if (message3.getCheck() && (message3.flag && ACK) && (message3.seqNum == seq + 1)) {
						seq++;
						std::cout << "---------- ���������ֳɹ��� ----------" << endl;
						return true;
					}
				}
				// ���յ�����Ϣ���ԣ������ǲ��ǵڶ������ֵı���û���յ���
				if (clock() - shake2start >= MAX_WAIT_TIME) {
					std::cout << "---------- �ڶ������ֳ�ʱ���������´��䣡 ----------" << endl;
					bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (SOCKADDR*)&clientAddr, addrLen);
					if (bytes == 0) {
						std::cout << "�ڶ�������ʱ����Ϣ���ͷ�������" << endl;
						return false;
					}
					shake2start = clock(); // ���¼�ʱ
				}				
			}
		}
	}
	return false;
}

int getAvailable(int start) {
	int available = WINDOW_SIZE;
	for (int i = 0; i < WINDOW_SIZE; i++) {
		if (start + i < sum) {
			if (isRecved[start + i] == true)
				available--;
		}
		else break;
	}
	return available;
}

bool waveHands()
{
	Message message1, message2, message3, message4;
	while (true) {
		// ��һ�λ��֣��ͻ��˷����ͷ���������FIN��λ��
		int bytes = recvfrom(serverSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "��һ�λ���ʱ����Ϣ���շ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// ��Ϣ���ճɹ�������������У��ͣ�FIN�ֶΣ��Լ�seq��
			if (!(message1.flag && FIN) || !message1.getCheck() || !(message1.seqNum == seq + 1)) {
				std::cout << "��һ�λ���ʱ��FIN�λ�У��Ͷλ����кŷ�������" << endl;
				return false;
			}
			// ��һ�λ��ֳɹ�
			std::cout << "---------- ��һ�λ��ֳɹ��� ----------" << endl;

			// �ڶ��λ��֣��������ͻ��˻�Ӧ��ֻ���ACK��λ��
			message2.setPort(SERVER_PORT, CLIENT_PORT);
			message2.ackNum = message1.seqNum;
			message2.set_ACK();
			message2.setCheck(); // ����У���
			bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&clientAddr, addrLen);
			if (bytes == 0) {
				std::cout << "�ڶ��λ���ʱ����Ϣ���ͷ�������" << endl;
				return false;
			}
			clock_t shake2start = clock();
			std::cout << "---------- �������ѷ��͵ڶ��λ��ֵ���Ϣ�� ----------" << endl;
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
		std::cout << "�����λ���ʱ����Ϣ���ͷ�������" << endl;
		return false;
	}
	std::cout << "---------- �������ѷ��͵����λ��ֵ���Ϣ�� ----------" << endl;

	// ���Ĵλ��֣��ͻ������������ȷ�ϣ�
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "���Ĵλ���ʱ����Ϣ���ͷ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// ��Ϣ���ճɹ�������������У��ͣ�ACK�ֶΣ��Լ�seq��
			if ((message4.flag && ACK) && message4.getCheck() && (message4.ackNum == message3.seqNum)) {
				std::cout << "---------- ���Ĵλ��ֳɹ��� ----------" << endl;
				return true;
			}
		}
		if (clock() - shake3start >= MAX_WAIT_TIME) {
			std::cout << "---------- �����λ��ֳ�ʱ���������´��䣡 ----------" << endl;
			bytes = sendto(serverSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&clientAddr, addrLen);
			shake3start = clock();
		}
	}
	return false;
}