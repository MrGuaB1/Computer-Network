#include <iostream>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include <sstream>
#include "segment.h"
#pragma comment(lib, "ws2_32.lib")

// 全局变量定义：
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // 初始化序列号
int addrLen = sizeof(serverAddr); // 用于UDP::recvfrom

// 函数声明：
bool shakeHands(); // UDP实现三次握手
bool waveHands(); // UDP实现四次挥手

// lab3-3新增：
int WINDOW_SIZE = 10;
int recv_flag = -1; // 标记最大有序接收的报文段编号
int left_space = WINDOW_SIZE; // 剩余可缓存的报文段
bool isRecved[LONGEST / MSS];
int sum = 0; // 总的报文段数量
int getAvailable(int start); // 获取以start开始的窗口中还有几个可缓存报文段

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
	serverSocket = socket(AF_INET, SOCK_DGRAM, 0); //数据报套接字，UDP协议
	unsigned long on = 1;
	ioctlsocket(serverSocket, FIONBIO, &on);

	// 填充地址信息结构体：
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(SERVER_PORT);
	inet_pton(AF_INET, LocalIP, &serverAddr.sin_addr.S_un.S_addr);

	clientAddr.sin_family = AF_INET;
	clientAddr.sin_port = htons(CLIENT_PORT);
	inet_pton(AF_INET, LocalIP, &clientAddr.sin_addr.S_un.S_addr);

	// 设置套接字选项：设置最长等待时间
	int timeout = 400;
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

	// 服务端绑定socket与addr：
	res = bind(serverSocket, (SOCKADDR*)&serverAddr, sizeof(SOCKADDR));
	if (res != 0) {
		std::cout << "服务端socket与地址信息绑定失败！" << endl;
		WSACleanup();
		return 1;
	}
	std::cout << "服务器资源已就绪，waiting for connecting ..." << endl;
	int size;
	std::cout << "input the window size you hope：";
	cin >> size;
	WINDOW_SIZE = size;
	
	// 三次握手建立连接
	bool alive = shakeHands();

	// 期待的序列号：
	int next = 0;
	
	// 开始文件传输，首先接收文件的总体信息：
	Message fileMessage; // 文件信息传输
	unsigned int fileSize; // 记录文件大小
	char fileName[100] = { 0 };
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&fileMessage, sizeof(fileMessage), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes > 0) {
			// 成功收到消息，检查ACK和seq，然后向客户端回复一个ACK：
			if (fileMessage.getCheck() && fileMessage.seqNum == next) {
				next++;
				fileSize = fileMessage.size;
				for (int i = 0; fileMessage.data[i]; i++)
					fileName[i] = fileMessage.data[i];
				std::cout << "服务端即将接收文件：" << fileName << "，其大小为：" << fileSize << endl;
				recv_flag++;
				isRecved[fileMessage.seqNum] = true;
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK(); //ACK置位表示响应
				ackMessage.ackNum = fileMessage.seqNum; // 回复的响应号就是接收的序列号
				ackMessage.setCheck(); // 设置校验和
				ackMessage.set_size(WINDOW_SIZE); // 设置接收窗口通告
				// 发送ACK报文
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
				std::cout << "服务器收到 seq = " << fileMessage.seqNum << "的【顺序】报文段，并发送 ack = " << ackMessage.ackNum << " 的回复报文段" << endl;
				break;
			}
		}
	}

	// 计算段长度：
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
				std::cout << "当前的recv_flag:" << recv_flag << endl;
				// 如果收到的报文段是有序的，那么向后检查，找到第一个false，即找到新的窗口开始位置
				if (recvMessage.seqNum == recv_flag + 1) {
					isRecved[recvMessage.seqNum] = true;
					for (int j = recv_flag; j < segments+1; j++) {
						if (isRecved[j] == false) {
							recv_flag = j - 1;
							break;
						}
					}
					// 向客户端发送顺序接收信息
					Message ackMessage;
					ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
					ackMessage.set_ACK(); 
					// 回复的响应号就是接收的序列号，表示期待发送端的 seqNum+1 的报文段
					ackMessage.ackNum = recvMessage.seqNum;
					ackMessage.setCheck(); // 设置校验和
					ackMessage.set_base(recv_flag + 1);
					left_space = getAvailable(recv_flag + 1);
					// 窗口大小通告
					ackMessage.set_size(left_space);
					// 发送ACK报文
					sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
					std::cout << "服务器收到 seq = " << recvMessage.seqNum << "的【顺序】报文段，并发送 ack = " << ackMessage.ackNum << " 的回复报文段" << endl;
					std::cout << "【滑动窗口信息打印】：服务端滑动窗口的开始位置：" << recv_flag + 1 << "，最多还可以缓存" << left_space << "个失序报文段" << endl;
					for (int j = 0; j < MSS; j++) 
						transFile[(recvMessage.seqNum - 1) * MSS + j] = recvMessage.data[j];
				}
				// 如果收到了乱序的报文段，那么要消耗一个可缓存数量，并且窗口不能发生移动
				else {
					// 对发来的报文段进行缓存
					isRecved[recvMessage.seqNum] = true;
					// 向客户端发送乱序接收信息
					Message ackMessage;
					ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
					ackMessage.set_ACK();
					ackMessage.ackNum = recvMessage.seqNum;
					ackMessage.setCheck(); // 设置校验和
					// 窗口大小通告
					left_space = getAvailable(recv_flag+1);
					ackMessage.set_base(recv_flag + 1);
					ackMessage.set_size(left_space);
					// 发送ACK报文
					sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
					std::cout << "服务器【缓存了】 seq = " << recvMessage.seqNum << "的【乱序】报文段，并发送 ack = " << ackMessage.ackNum << " 的回复报文段" << endl;
					std::cout << "【滑动窗口信息打印】：服务端滑动窗口的开始位置：" << recv_flag + 1 << "，最多还可以缓存" << left_space << "个失序报文段" << endl;
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
				std::cout << "数据报" << recvMessage.seqNum << "接收成功！" << endl;
				isRecved[recvMessage.seqNum] = true;
				Message ackMessage;
				ackMessage.setPort(SERVER_PORT, CLIENT_PORT);
				ackMessage.set_ACK();
				ackMessage.ackNum = recvMessage.seqNum;
				ackMessage.setCheck();
				// 窗口大小通告
				ackMessage.set_size(left_space);
				// 发送ACK报文
				sendto(serverSocket, (char*)&ackMessage, sizeof(ackMessage), 0, (sockaddr*)&clientAddr, addrLen);
				for (int j = 0; j < leftBytes; j++)
					transFile[segments * MSS + j] = recvMessage.data[j];
				break;
			}
		}
	}

	std::cout << "全部数据报接收完毕，正在写入文件！" << endl;
	FILE* newFile;
	newFile = fopen(fileName, "wb");
	std::fwrite(transFile, fileSize, 1, newFile);
	std::fclose(newFile);
	std::cout << "写入文件完毕！" << endl;

	// 四次挥手释放连接：
	waveHands();
	std::cout << "连接已成功释放！" << endl;

	WSACleanup();
	std::system("pause");
	return 0;
}

bool shakeHands()
{
	// 三次握手过程种的报文缓冲区
	Message message1, message2, message3;
	while (true) {
		// 第一次握手：客户端向服务端握手，并把SYN置位
		int bytes = recvfrom(serverSocket, (char*)&message1, sizeof(message1), 0, (SOCKADDR*)&clientAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "第一次握手时，信息接收发生错误！" << endl;
			return false;
		}
		else if (bytes > 0) {
			// 成功接收到信息，接下来检验校验和，SYN字段，以及seq：
			// TCP协议中，规定SYN报文段不能携带数据，但需要消耗一个序列号，因此seq在正常情况下要+1
			if (!(message1.flag && SYN) || !(message1.getCheck()) || !(message1.seqNum == seq + 1)) {
				std::cout << "第一次握手时，SYN段或校验和段或序列号发生错误！" << endl;
				return false;
			}
			// 第一次握手成功
			std::cout << "---------- 第一次握手成功！ ----------" << endl;

			// 第二次握手：服务端返回信息给客户端，序列号+1，SYN，ACK置位
			seq++;
			message2.setPort(SERVER_PORT, CLIENT_PORT); // 绑定端口信息
			// ack号就是发来的seq号
			message2.ackNum = message1.seqNum;
			// 将SYN和ACK置位，然后发送回复消息
			message2.set_ACK();
			message2.set_SYN();
			message2.setCheck(); // 设置校验和
			bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (SOCKADDR*)&clientAddr, addrLen);
			if (bytes == 0) {
				std::cout << "第二次握手时，信息发送发生错误！" << endl;
				return false;
			}
			clock_t shake2start = clock(); // 第二次握手的信息如果超时需要重传
			std::cout << "---------- 服务器已发送第二次握手的消息，等待第三次握手 ----------" << endl;

			// 第三次握手：客户端返回信息给服务端，SYN无需置位，ACK需要置位：
			while (true) {
				bytes = recvfrom(serverSocket, (char*)&message3, sizeof(message3), 0, (SOCKADDR*)&clientAddr, &addrLen);
				if (bytes == 0) {
					std::cout << "第三次握手时，信息接收发生错误！" << endl;
					return false;
				}
				else if (bytes > 0) {
					// 成功接收到信息，接下来检验校验和，SYN字段，以及seq：
					if (message3.getCheck() && (message3.flag && ACK) && (message3.seqNum == seq + 1)) {
						seq++;
						std::cout << "---------- 第三次握手成功！ ----------" << endl;
						return true;
					}
				}
				// 接收到的信息不对，考虑是不是第二次握手的报文没有收到：
				if (clock() - shake2start >= MAX_WAIT_TIME) {
					std::cout << "---------- 第二次握手超时，正在重新传输！ ----------" << endl;
					bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (SOCKADDR*)&clientAddr, addrLen);
					if (bytes == 0) {
						std::cout << "第二次握手时，信息发送发生错误！" << endl;
						return false;
					}
					shake2start = clock(); // 重新计时
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
		// 第一次挥手，客户端发送释放连接请求，FIN置位：
		int bytes = recvfrom(serverSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "第一次挥手时，信息接收发生错误！" << endl;
			return false;
		}
		else if (bytes > 0) {
			// 信息接收成功，接下来检验校验和，FIN字段，以及seq：
			if (!(message1.flag && FIN) || !message1.getCheck() || !(message1.seqNum == seq + 1)) {
				std::cout << "第一次挥手时，FIN段或校验和段或序列号发生错误！" << endl;
				return false;
			}
			// 第一次挥手成功
			std::cout << "---------- 第一次挥手成功！ ----------" << endl;

			// 第二次挥手，服务端向客户端回应，只需把ACK置位：
			message2.setPort(SERVER_PORT, CLIENT_PORT);
			message2.ackNum = message1.seqNum;
			message2.set_ACK();
			message2.setCheck(); // 设置校验和
			bytes = sendto(serverSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&clientAddr, addrLen);
			if (bytes == 0) {
				std::cout << "第二次挥手时，信息发送发生错误！" << endl;
				return false;
			}
			clock_t shake2start = clock();
			std::cout << "---------- 服务器已发送第二次挥手的消息！ ----------" << endl;
			break;
		}
	}

	// 第三次挥手，服务端发送释放报文，将FIN和ACK置位，并等待客户端发来的确认报文
	message3.setPort(SERVER_PORT, CLIENT_PORT);
	message3.set_ACK();
	message3.set_FIN();
	message3.seqNum = seq++;
	message3.setCheck(); // 设置校验和
	int bytes = sendto(serverSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&clientAddr, addrLen);
	clock_t shake3start = clock();
	if (bytes == 0) {
		std::cout << "第三次挥手时，信息发送发生错误！" << endl;
		return false;
	}
	std::cout << "---------- 服务器已发送第三次挥手的消息！ ----------" << endl;

	// 第四次挥手：客户端向服务端最后确认：
	while (true) {
		int bytes = recvfrom(serverSocket, (char*)&message4, sizeof(message4), 0, (sockaddr*)&clientAddr, &addrLen);
		if (bytes == 0) {
			std::cout << "第四次挥手时，信息发送发生错误！" << endl;
			return false;
		}
		else if (bytes > 0) {
			// 信息接收成功，接下来检验校验和，ACK字段，以及seq：
			if ((message4.flag && ACK) && message4.getCheck() && (message4.ackNum == message3.seqNum)) {
				std::cout << "---------- 第四次挥手成功！ ----------" << endl;
				return true;
			}
		}
		if (clock() - shake3start >= MAX_WAIT_TIME) {
			std::cout << "---------- 第三次挥手超时，正在重新传输！ ----------" << endl;
			bytes = sendto(serverSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&clientAddr, addrLen);
			shake3start = clock();
		}
	}
	return false;
}