#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include "segment.h"
#pragma comment (lib, "ws2_32.lib")

// ȫ�ֱ������壺
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // ��ʼ�����к�
int addrLen = sizeof(clientAddr); // ����UDP::recvfrom
BYTE* transFile = new BYTE[LONGEST];

// lab3-2������
int window_start = 0; // �������ڵĿ�ʼλ�ã��������
int next_send = 0; // ָ����һ��Ҫ���͵����к�
bool isEnd = 0; // �ж��Ƿ�������
//bool quickSend = 0; // �����ظ�ACK�����ش�
clock_t msgTime = 0; // ���ķ����ش�ʱ��
int WINDOW_SIZE = 10; // �������ڴ�С
std::mutex mtx; // ����������


// lab3-3������
bool isRecved[LONGEST / MSS];
int sum; // ��¼�ܵı��Ķ�����
int server_base = 0;
int server_available = WINDOW_SIZE; // ����˿��ÿռ�
DWORD WINAPI sendThread(PVOID lpParam);
DWORD WINAPI resendCheck(PVOID lpParam);
int vaild_thread = 0; // ���ͬʱ�������߳���
bool check_flag = true;

// ����������
bool shakeHands(); // UDPʵ����������
bool waveHands(); // UDPʵ���Ĵλ���
void sendSegment(Message&); // �������ݶ�
void sendFile(string);

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
	int timeout = 400; // ��ʱʱ�䣨�Ժ���Ϊ��λ��
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	// ����˰���Ϣ��
	bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));

	// �������ֽ������ӣ�
	bool alive = shakeHands();
	std::cout << "�������ֽ������ӳɹ������ڿ���ѡ���ϴ��ļ���Ͽ�����" << endl;

	// ��ʼ�ļ�����

	int which, size;
	std::cout << "input the window size you hope��";
	cin >> size;
	WINDOW_SIZE = size;
	std::cout << "input 1-3 to transport 1-3.jpg or input 4 to transport helloworld.txt��";
	cin >> which;
	switch (which)
	{
	case 1:sendFile("1.jpg"); break;
	case 2:sendFile("2.jpg"); break;
	case 3:sendFile("3.jpg"); break;
	case 4:sendFile("helloworld.txt"); break;
	}
	std::cout << "���˳����䣬׼���Ĵλ����ͷ����ӣ�" << endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(400));

	//�Ĵλ����ͷ����ӣ�
	waveHands();

	std::cout << "���ͷ����ӣ�" << endl;
	WSACleanup();
	system("pause");
	return 0;
}

DWORD WINAPI resendCheck(PVOID lpParam)
{
	std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
	int* pointer = (int*)lpParam;
	int pid = *pointer;
	while (true) {
		// ��ʱ�ش�
		if (std::chrono::steady_clock::now() - startTime >= std::chrono::milliseconds(400)) {
			Message sendMessage;
			sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
			sendMessage.seqNum = pid;
			// �������
			for (int j = 0; j < MSS; j++)
				sendMessage.data[j] = transFile[(pid - 1) * MSS + j];
			sendMessage.setCheck();
			std::cout << "�ͻ����ѡ���ʱ�ش���seq = " << sendMessage.seqNum << "�ı��ĶΣ�" << endl;
			sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
			isRecved[pid] = true;
		}
		if (isRecved[pid] == true) {
			return 0;
		}
	}
	return 0;
}

void sendSegment(Message& sendMessage)
{
	// ���ͱ��ĶΣ�����ʼ��ʱ
	sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
	HANDLE resendThread = CreateThread(NULL, 0, resendCheck, &sendMessage.seqNum, 0, NULL);
	clock_t startTime = clock();
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::cout << "�ͻ����ѷ��� seq = " << sendMessage.seqNum << " �ı��ĶΣ�" << endl;
		std::cout << "������������Ϣ��ӡ�����ڿ�ʼλ�ã�" << window_start << "����һ��Ҫ���͵ı��Ķ���ţ�"<<next_send<<"��Ŀǰ�����ѷ��͵�δ�յ�ACK�ı��Ķ�������" << next_send - window_start << "����δ���͵ı��Ķ�������" << WINDOW_SIZE - (next_send - window_start) << endl;
	}
	Message recvMessage;
	while (check_flag == true) {
		int bytes = recvfrom(clientSocket, (char*)&recvMessage, sizeof(recvMessage), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			if (recvMessage.ackNum == sendMessage.seqNum) {
				{			
					server_available = recvMessage.recv_size;
					std::lock_guard<std::mutex> lock(mtx);
					isRecved[sendMessage.seqNum] = true;
					std::cout << "�ͻ����յ��˷���˷����� ack = " << recvMessage.ackNum << " ��ȷ�ϱ���" << endl;						
					// ���ǰ�涼�Ѿ������ˣ���ô�������ڽ����ƶ�
					if (sendMessage.seqNum == window_start) {
						for (int j = window_start; j < sum; j++) {
							if (isRecved[j] == false && isRecved[j-1] == true)
								window_start = j;						
						}
					}
					server_base = recvMessage.base;
					if (server_base > window_start) {
						window_start = server_base;
					}
					vaild_thread--;
					return;
				}
			}
		}
		//if (clock() - startTime >= MAX_WAIT_TIME) {
		//	std::cout << "seq = " << sendMessage.seqNum << " �ı��Ķη��ͳ�ʱ���ͻ����������´��� ......" << endl;
		//	sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
		//	startTime = clock();
		//}
	}
}

DWORD WINAPI sendThread(PVOID lpParam)
{
	Message& sendMessage = *(Message*)lpParam;
	sendSegment(sendMessage);
	return 0;
}

void sendFile(string fileName)
{
	int startTime = clock();
	ifstream fin(fileName.c_str(), ifstream::binary); // ���ֽ����򿪴����ļ�
	unsigned int fileSize = 0;
	BYTE currByte = fin.get();
	while (fin) {
		transFile[fileSize++] = currByte;
		currByte = fin.get();
	}
	fin.close();

	// ����γ��Ⱥ��ܱ��Ķ�������
	int segments = fileSize / MSS;
	int leftBytes = fileSize % MSS;
	sum = leftBytes > 0 ? segments + 2 : segments + 1; // �ļ���ϢҲҪռһ�����Ķ�

	// �ȷ����ļ���Ϣ
	Message fileMessage;
	fileMessage.setPort(CLIENT_PORT, SERVER_PORT);
	fileMessage.size = fileSize;
	fileMessage.seqNum = 0;
	for(int i=0;i<fileName.size();i++)
		fileMessage.data[i] = fileName[i];
	fileMessage.data[fileName.size()] = '\0';
	fileMessage.setCheck();
	msgTime = clock();
	sendto(clientSocket, (char*)&fileMessage, sizeof(fileMessage), 0, (SOCKADDR*)&serverAddr, addrLen);
	next_send++;
	std::cout << "�ͻ����ѷ��� seq = " << fileMessage.seqNum << " �ı��ĶΣ�" << endl;
	std::cout << "������������Ϣ��ӡ�����ڿ�ʼλ�ã�" << window_start << "����һ��Ҫ���͵ı��Ķ���ţ�" << next_send << "��Ŀǰ�����ѷ��͵�δ�յ�ACK�ı��Ķ�������" << next_send - window_start << "����δ���͵ı��Ķ�������" << WINDOW_SIZE - (next_send - window_start) << endl;
	window_start++;
	isRecved[0] = true;

	// int n = (WINDOW_SIZE > 10) ?  10 : WINDOW_SIZE;
	// ѡ���ش������㴫������ʱ��Զ�����ÿ����ά��һ���߳�
	HANDLE threadHandles[LONGEST/MSS];
	for (int i = 0; i < segments; i++) {
		while (true) {
			// �����������������Ͷ��п��ô�С�����ն��п��ô�С
			if (next_send < window_start + WINDOW_SIZE && server_available > 0 && vaild_thread < WINDOW_SIZE) {
				Message sendMessage;
				sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
				sendMessage.seqNum = next_send;
				// �������
				for (int j = 0; j < MSS; j++)
					sendMessage.data[j] = transFile[(next_send - 1) * MSS + j];
				sendMessage.setCheck();
				next_send++;
				// ���������߳�
				threadHandles[i] = CreateThread(NULL, 0, sendThread, &sendMessage, 0, NULL);
				{
					std::lock_guard<std::mutex> lock(mtx);
					vaild_thread++;
				}			
				std::this_thread::sleep_for(std::chrono::milliseconds(100));
				break;
			}
		}
	}
	check_flag = false;
	for (int i = 0; i < segments; i++) 
		WaitForSingleObject(threadHandles[i], INFINITE);

	if (leftBytes > 0) {
		Message sendMessage;
		sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
		sendMessage.seqNum = segments + 1;
		for (int j = 0; j < leftBytes; j++)
			sendMessage.data[j] = transFile[segments * MSS + j];
		sendMessage.setCheck();
		{
			std::lock_guard<std::mutex> lock(mtx);			
			sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (SOCKADDR*)&serverAddr, addrLen);
			std::cout << "�ͻ����ѷ��� seq = " << sendMessage.seqNum << " �ı��ĶΣ�" << endl;
			std::cout << "������������Ϣ��ӡ�����ڴ�С��" << WINDOW_SIZE << "��Ŀǰ�����ѷ��͵�δ�յ�ACK�ı��Ķ�������" << next_send - window_start << "����δ���͵ı��Ķ�������" << WINDOW_SIZE - (next_send - window_start) << endl;
		}
		isRecved[segments + 1] = true;
	}
	clock_t endTime = clock();
	std::cout << "�ļ�" << fileName << "���ܴ���ʱ��Ϊ��" << (endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
	std::cout << "������̵�������Ϊ:" << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << "byte/s" << endl << endl;
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
		std::cout << "��һ������ʱ����Ϣ���ͷ�������" << endl;
		return false;
	}
	std::cout << "---------- �ͻ����ѷ��͵�һ�����ֵ���Ϣ���ȴ��ڶ������� ----------" << endl;

	// �ڶ������֣�����˷�����Ϣ���ͻ��ˣ�����SYN��ACK��λ��
	while (true) {
		int bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "�ڶ�������ʱ����Ϣ���շ�������" << endl;
			return false;
		}
		else if (bytes > 0) {
			// �ɹ����յ���Ϣ������������У��ͣ�SYN��ACK�ֶΣ��Լ�seq��
			if ((message2.flag && SYN) && (message2.flag && ACK) && message2.getCheck() && message2.ackNum == message1.seqNum) {
				std::cout << "---------- �ڶ������ֳɹ��� ----------" << endl;
				break;
			}
		}
		if (clock() - shake1 >= MAX_WAIT_TIME) {
			std::cout << "---------- ��һ�����ֳ�ʱ���������´��䣡 ----------" << endl;
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
	std::cout << "---------- �ͻ����ѷ��͵��������ֵ���Ϣ����׼������ ----------" << endl;
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
		std::cout << "��һ�λ���ʱ����Ϣ���ͷ�������" << endl;
		return false;
	}
	clock_t wave1 = clock();
	std::cout << "---------- �ͻ����ѷ��͵�һ�λ��ֵ���Ϣ���ȴ��ڶ��λ��� ----------" << endl;

	// �ڶ��λ��֣��������ͻ��ˣ�ACK��λ��
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// �ɹ����յ���Ϣ������������У��ͣ�ACK�ֶΣ��Լ�seq��
			if (message2.getCheck() && (message2.flag && ACK) && message2.ackNum == message1.seqNum) {
				std::cout << "---------- �ڶ��λ��ֳɹ��� ----------" << endl;
				break;
			}
		}
		if (clock() - wave1 >= MAX_WAIT_TIME) {
			std::cout << "---------- ��һ�λ��ֳ�ʱ���������´��䣡 ----------" << endl;
			bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
			wave1 = clock();
		}
	}

	// �����λ��֣��������ͻ��˷����ͷ����ӱ��ģ�FIN��ACK��λ��
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			if (message3.getCheck() && (message3.flag && FIN) && (message3.flag && ACK)) {
				std::cout << "---------- �����λ��ֳɹ��� ----------" << endl;
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
	std::cout << "---------- �ͻ����ѷ��͵��Ĵλ��ֵ���Ϣ����Դ׼���رգ� ----------" << endl;

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
	std::cout << "---------- ��Դ�ͷ���ϣ����ӶϿ��� ----------" << endl;
}

DWORD WINAPI checkThread(PVOID lpParam)
{
	int* pointer = (int*)lpParam;
	int segments = *pointer;
	while (check_flag == true) {
		std::this_thread::sleep_for(std::chrono::milliseconds(400));
		for (int i = 0; i < segments - 1; i++) {
			if (isRecved[i] == false && isRecved[i + 1] == true) {
				Message sendMessage;
				sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
				sendMessage.seqNum = i;
				// �������
				for (int j = 0; j < MSS; j++)
					sendMessage.data[j] = transFile[(i - 1) * MSS + j];
				sendMessage.setCheck();
				std::cout << "�ͻ����ѡ���ʱ�ش���seq = " << sendMessage.seqNum << "�ı��ĶΣ�" << endl;
				sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
				isRecved[i] = true;
				//if (server_base > window_start) {
				//	window_start = server_base;
				//}
				break;
			}
		}
	}
	return 0;
}