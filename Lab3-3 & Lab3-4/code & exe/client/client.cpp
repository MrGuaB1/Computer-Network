#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <chrono>
#include <thread>
#include <mutex>
#include "segment.h"
#pragma comment (lib, "ws2_32.lib")

// 全局变量定义：
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // 初始化序列号
int addrLen = sizeof(clientAddr); // 用于UDP::recvfrom
BYTE* transFile = new BYTE[LONGEST];

// lab3-2新增：
int window_start = 0; // 滑动窗口的开始位置，即基序号
int next_send = 0; // 指向下一个要发送的序列号
bool isEnd = 0; // 判断是否接收完毕
//bool quickSend = 0; // 三次重复ACK快速重传
clock_t msgTime = 0; // 报文分组重传时间
int WINDOW_SIZE = 10; // 滑动窗口大小
std::mutex mtx; // 互斥锁对象


// lab3-3新增：
bool isRecved[LONGEST / MSS];
int sum; // 记录总的报文段数量
int server_base = 0;
int server_available = WINDOW_SIZE; // 服务端可用空间
DWORD WINAPI sendThread(PVOID lpParam);
DWORD WINAPI resendCheck(PVOID lpParam);
int vaild_thread = 0; // 最多同时工作的线程数
bool check_flag = true;

// 函数声明：
bool shakeHands(); // UDP实现三次握手
bool waveHands(); // UDP实现四次挥手
void sendSegment(Message&); // 发送数据段
void sendFile(string);

using namespace std;
int main()
{
	// 初始化winsock库
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (res != 0) {
		std::cout << "winsock库初始化失败！";
		return 1;
	}

	// 新建socket：
	clientSocket = socket(AF_INET, SOCK_DGRAM, 0); //数据报套接字，UDP协议
	unsigned long on = 1;
	ioctlsocket(clientSocket, FIONBIO, &on);

	// 填充地址信息结构体：
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, LocalIP, &serverAddr.sin_addr.S_un.S_addr);

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, LocalIP, &clientAddr.sin_addr.S_un.S_addr);

	// 设置套接字选项：设置最长等待时间
	int timeout = 400; // 超时时间（以毫秒为单位）
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	// 服务端绑定信息：
	bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));

	// 三次握手建立连接：
	bool alive = shakeHands();
	std::cout << "三次握手建立连接成功，现在可以选择上传文件或断开连接" << endl;

	// 开始文件传输

	int which, size;
	std::cout << "input the window size you hope：";
	cin >> size;
	WINDOW_SIZE = size;
	std::cout << "input 1-3 to transport 1-3.jpg or input 4 to transport helloworld.txt：";
	cin >> which;
	switch (which)
	{
	case 1:sendFile("1.jpg"); break;
	case 2:sendFile("2.jpg"); break;
	case 3:sendFile("3.jpg"); break;
	case 4:sendFile("helloworld.txt"); break;
	}
	std::cout << "已退出传输，准备四次挥手释放连接！" << endl;
	std::this_thread::sleep_for(std::chrono::milliseconds(400));

	//四次挥手释放连接：
	waveHands();

	std::cout << "已释放连接！" << endl;
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
		// 超时重传
		if (std::chrono::steady_clock::now() - startTime >= std::chrono::milliseconds(400)) {
			Message sendMessage;
			sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
			sendMessage.seqNum = pid;
			// 填充数据
			for (int j = 0; j < MSS; j++)
				sendMessage.data[j] = transFile[(pid - 1) * MSS + j];
			sendMessage.setCheck();
			std::cout << "客户端已【超时重传】seq = " << sendMessage.seqNum << "的报文段！" << endl;
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
	// 发送报文段，并开始计时
	sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
	HANDLE resendThread = CreateThread(NULL, 0, resendCheck, &sendMessage.seqNum, 0, NULL);
	clock_t startTime = clock();
	{
		std::lock_guard<std::mutex> lock(mtx);
		std::cout << "客户端已发送 seq = " << sendMessage.seqNum << " 的报文段！" << endl;
		std::cout << "【滑动窗口信息打印】窗口开始位置：" << window_start << "，下一个要发送的报文段序号："<<next_send<<"，目前存在已发送但未收到ACK的报文段数量：" << next_send - window_start << "，还未发送的报文段数量：" << WINDOW_SIZE - (next_send - window_start) << endl;
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
					std::cout << "客户端收到了服务端发来的 ack = " << recvMessage.ackNum << " 的确认报文" << endl;						
					// 如果前面都已经有序了，那么滑动窗口进行移动
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
		//	std::cout << "seq = " << sendMessage.seqNum << " 的报文段发送超时，客户端正在重新传输 ......" << endl;
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
	ifstream fin(fileName.c_str(), ifstream::binary); // 以字节流打开传入文件
	unsigned int fileSize = 0;
	BYTE currByte = fin.get();
	while (fin) {
		transFile[fileSize++] = currByte;
		currByte = fin.get();
	}
	fin.close();

	// 计算段长度和总报文端数量：
	int segments = fileSize / MSS;
	int leftBytes = fileSize % MSS;
	sum = leftBytes > 0 ? segments + 2 : segments + 1; // 文件信息也要占一个报文段

	// 先发送文件信息
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
	std::cout << "客户端已发送 seq = " << fileMessage.seqNum << " 的报文段！" << endl;
	std::cout << "【滑动窗口信息打印】窗口开始位置：" << window_start << "，下一个要发送的报文段序号：" << next_send << "，目前存在已发送但未收到ACK的报文段数量：" << next_send - window_start << "，还未发送的报文段数量：" << WINDOW_SIZE - (next_send - window_start) << endl;
	window_start++;
	isRecved[0] = true;

	// int n = (WINDOW_SIZE > 10) ?  10 : WINDOW_SIZE;
	// 选择重传，满足传输条件时相对独立，每个包维护一个线程
	HANDLE threadHandles[LONGEST/MSS];
	for (int i = 0; i < segments; i++) {
		while (true) {
			// 两个必须条件：发送端有可用大小、接收端有可用大小
			if (next_send < window_start + WINDOW_SIZE && server_available > 0 && vaild_thread < WINDOW_SIZE) {
				Message sendMessage;
				sendMessage.setPort(CLIENT_PORT, SERVER_PORT);
				sendMessage.seqNum = next_send;
				// 填充数据
				for (int j = 0; j < MSS; j++)
					sendMessage.data[j] = transFile[(next_send - 1) * MSS + j];
				sendMessage.setCheck();
				next_send++;
				// 创建发送线程
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
			std::cout << "客户端已发送 seq = " << sendMessage.seqNum << " 的报文段！" << endl;
			std::cout << "【滑动窗口信息打印】窗口大小：" << WINDOW_SIZE << "，目前存在已发送但未收到ACK的报文段数量：" << next_send - window_start << "，还未发送的报文段数量：" << WINDOW_SIZE - (next_send - window_start) << endl;
		}
		isRecved[segments + 1] = true;
	}
	clock_t endTime = clock();
	std::cout << "文件" << fileName << "的总传输时间为：" << (endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
	std::cout << "传输过程的吞吐率为:" << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << "byte/s" << endl << endl;
}

bool shakeHands()
{
	Message message1, message2, message3;
	// 第一次握手，客户端向服务端发起请求，并把SYN置位：
	message1.setPort(CLIENT_PORT, SERVER_PORT);
	message1.set_SYN();
	message1.seqNum = ++seq;
	message1.setCheck();
	int bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
	clock_t shake1 = clock();
	if (bytes == 0) {
		std::cout << "第一次握手时，信息发送发生错误！" << endl;
		return false;
	}
	std::cout << "---------- 客户端已发送第一次握手的消息，等待第二次握手 ----------" << endl;

	// 第二次握手，服务端发送信息到客户端，并把SYN和ACK置位：
	while (true) {
		int bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "第二次握手时，信息接收发生错误！" << endl;
			return false;
		}
		else if (bytes > 0) {
			// 成功接收到信息，接下来检验校验和，SYN和ACK字段，以及seq：
			if ((message2.flag && SYN) && (message2.flag && ACK) && message2.getCheck() && message2.ackNum == message1.seqNum) {
				std::cout << "---------- 第二次握手成功！ ----------" << endl;
				break;
			}
		}
		if (clock() - shake1 >= MAX_WAIT_TIME) {
			std::cout << "---------- 第一次握手超时，正在重新传输！ ----------" << endl;
			int bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
			shake1 = clock(); // 重新计时
		}
	}

	// 第三次握手，客户端向服务端发起，只把ACK置位：
	message3.setPort(CLIENT_PORT, SERVER_PORT);
	message3.set_ACK();
	message3.seqNum = ++seq;
	message3.setCheck();
	bytes = sendto(clientSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&serverAddr, addrLen);
	std::cout << "---------- 客户端已发送第三次握手的消息，已准备就绪 ----------" << endl;
	return true;
}

bool waveHands()
{
	Message message1, message2, message3, message4;
	// 第一次挥手，客户端向服务端发起，并把FIN置位：
	message1.setPort(CLIENT_PORT, SERVER_PORT);
	message1.set_FIN();
	message1.seqNum = ++seq;
	message1.setCheck();
	int bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
	if (bytes == 0) {
		std::cout << "第一次挥手时，信息发送发生错误！" << endl;
		return false;
	}
	clock_t wave1 = clock();
	std::cout << "---------- 客户端已发送第一次挥手的消息，等待第二次挥手 ----------" << endl;

	// 第二次挥手，服务端向客户端，ACK置位：
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// 成功接收到信息，接下来检验校验和，ACK字段，以及seq：
			if (message2.getCheck() && (message2.flag && ACK) && message2.ackNum == message1.seqNum) {
				std::cout << "---------- 第二次挥手成功！ ----------" << endl;
				break;
			}
		}
		if (clock() - wave1 >= MAX_WAIT_TIME) {
			std::cout << "---------- 第一次挥手超时，正在重新传输！ ----------" << endl;
			bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
			wave1 = clock();
		}
	}

	// 第三次挥手，服务端向客户端发送释放连接报文，FIN和ACK置位：
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			if (message3.getCheck() && (message3.flag && FIN) && (message3.flag && ACK)) {
				std::cout << "---------- 第三次挥手成功！ ----------" << endl;
				break;
			}
		}
	}

	// 第四次挥手，客户端向服务端最终确认，并把ACK置位：
	message4.setPort(CLIENT_PORT, SERVER_PORT);
	message4.set_ACK();
	message4.ackNum = message3.seqNum;
	message4.setCheck();
	bytes = sendto(clientSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&serverAddr, addrLen);
	std::cout << "---------- 客户端已发送第四次挥手的消息，资源准备关闭！ ----------" << endl;

	// 客户端还必须等待2*MSL，防止最后一个ACK还未到达：
	clock_t waitFor2MSL = clock();
	Message ackMessage;
	while (clock() - waitFor2MSL < 2 * MAX_WAIT_TIME) {
		int bytes = recvfrom(clientSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// 重发最后一个ACK，并重新计时2*MSL
			sendto(clientSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&serverAddr, addrLen);
			waitFor2MSL = clock();
		}
	}
	std::cout << "---------- 资源释放完毕，连接断开！ ----------" << endl;
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
				// 填充数据
				for (int j = 0; j < MSS; j++)
					sendMessage.data[j] = transFile[(i - 1) * MSS + j];
				sendMessage.setCheck();
				std::cout << "客户端已【超时重传】seq = " << sendMessage.seqNum << "的报文段！" << endl;
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