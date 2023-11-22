#include <iostream>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <fstream>
#include "segment.h"
#pragma comment (lib, "ws2_32.lib")

// 全局变量定义：
SOCKET serverSocket, clientSocket;
SOCKADDR_IN serverAddr, clientAddr;
const char* LocalIP = "127.0.0.1";
int seq = 0; // 初始化序列号
int addrLen = sizeof(clientAddr); // 用于UDP::recvfrom

// 函数声明：
bool shakeHands(); // UDP实现三次握手
bool waveHands(); // UDP实现四次挥手
bool sendSegment(Message&); // 发送数据段
bool sendFile(string);

using namespace std;
int main()
{
	// 初始化winsock库
	WSADATA wsadata;
	int res = WSAStartup(MAKEWORD(2, 2), &wsadata);
	if (res != 0) {
		cout << "winsock库初始化失败！";
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
	setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, (char*)&MAX_WAIT_TIME, sizeof(MAX_WAIT_TIME));
	
	// 服务端绑定信息：
	bind(clientSocket, (LPSOCKADDR)&clientAddr, sizeof(clientAddr));

	// 三次握手建立连接：
	bool alive = shakeHands();
	cout << "三次握手建立连接成功，现在可以选择上传文件或断开连接" << endl;
	
	// 开始文件传输

	int which;
	cout << "input 1-3 to transport 1-3.jpg or input 4 to transport helloworld.txt：";
	cin >> which;
	switch (which) 
	{
		case 1:sendFile("1.jpg"); break;
		case 2:sendFile("2.jpg"); break;
		case 3:sendFile("3.jpg"); break;
		case 4:sendFile("helloworld.txt"); break;
	}
	cout << "已退出传输，准备四次挥手释放连接！" << endl;

	//四次挥手释放连接：
	waveHands();

	cout << "已释放连接！" << endl;
	WSACleanup();
	system("pause");
	return 0;
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
		cout << "第一次握手时，信息发送发生错误！" << endl;
		return false;
	}
	cout << "---------- 客户端已发送第一次握手的消息，等待第二次握手 ----------" << endl;

	// 第二次握手，服务端发送信息到客户端，并把SYN和ACK置位：
	while (true) {
		int bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes == 0) {
			cout << "第二次握手时，信息接收发生错误！" << endl;
			return false;
		}
		else if (bytes > 0) {
			// 成功接收到信息，接下来检验校验和，SYN和ACK字段，以及seq：
			if ((message2.flag && SYN) && (message2.flag && ACK) && message2.getCheck() && message2.ackNum == message1.seqNum) {
				cout << "---------- 第二次握手成功！ ----------" << endl;
				break;
			}
		}
		if (clock() - shake1 >= MAX_WAIT_TIME) {
			cout << "---------- 第一次握手超时，正在重新传输！ ----------" << endl;
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
	cout << "---------- 客户端已发送第三次握手的消息，已准备就绪 ----------" << endl;
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
		cout << "第一次挥手时，信息发送发生错误！" << endl;
		return false;
	}
	clock_t wave1 = clock();
	cout << "---------- 客户端已发送第一次挥手的消息，等待第二次挥手 ----------" << endl;

	// 第二次挥手，服务端向客户端，ACK置位：
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message2, sizeof(message2), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// 成功接收到信息，接下来检验校验和，ACK字段，以及seq：
			if (message2.getCheck() && (message2.flag && ACK) && message2.ackNum == message1.seqNum) {
				cout << "---------- 第二次挥手成功！ ----------" << endl;
				break;
			}
		}
		if (clock() - wave1 >= MAX_WAIT_TIME) {
			cout << "---------- 第一次挥手超时，正在重新传输！ ----------" << endl;
			bytes = sendto(clientSocket, (char*)&message1, sizeof(message1), 0, (sockaddr*)&serverAddr, addrLen);
			wave1 = clock();
		}
	}

	// 第三次挥手，服务端向客户端发送释放连接报文，FIN和ACK置位：
	while (true) {
		bytes = recvfrom(clientSocket, (char*)&message3, sizeof(message3), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			if (message3.getCheck() && (message3.flag && FIN) && (message3.flag && ACK)) {
				cout << "---------- 第三次挥手成功！ ----------" << endl;
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
	cout << "---------- 客户端已发送第四次挥手的消息，资源准备关闭！ ----------" << endl;

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
	cout << "---------- 资源释放完毕，连接断开！ ----------" << endl;
}

bool sendSegment(Message& sendMessage)
{
	// 发送报文段，并开始计时
	sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr, addrLen);
	clock_t send_time = clock();
	cout << "客户端发送了 seq = " << sendMessage.seqNum << " 的报文段" << endl;
	Message recvMessage;
	while (true) {
		int bytes = recvfrom(clientSocket, (char*)&recvMessage, sizeof(recvMessage), 0, (sockaddr*)&serverAddr, &addrLen);
		if (bytes > 0) {
			// 成功收到消息，检查ACK和seq，但无需向服务端回复一个ACK
			if ((recvMessage.flag && ACK) && (recvMessage.ackNum == sendMessage.seqNum)) {
				cout << "客户端收到了服务端发来的 ack = " << recvMessage.ackNum << " 的确认报文" << endl;
				return true;
			}
		}
		if (clock() - send_time >= MAX_WAIT_TIME) {
			cout << "seq = " << sendMessage.seqNum << " 的报文段发送超时，客户端正在重新传输 ......" << endl;
			sendto(clientSocket, (char*)&sendMessage, sizeof(sendMessage), 0, (sockaddr*)&serverAddr,addrLen);
			send_time = clock();
		}
	}
	return false;
}

bool sendFile(string fileName)
{
	int startTime = clock();
	ifstream fin(fileName.c_str(), ifstream::binary); // 以字节流打开传入文件
	BYTE* transFile = new BYTE[LONGEST];
	unsigned int fileSize = 0;
	BYTE currByte = fin.get();
	while (fin) {
		transFile[fileSize++] = currByte;
		currByte = fin.get();
	}
	fin.close();
	
	// 先发送文件基本信息：
	Message fileMessage;
	fileMessage.setPort(CLIENT_PORT, SERVER_PORT);
	fileMessage.size = fileSize;
	fileMessage.seqNum = ++seq;
	// 将文件名填入信息的data字段：
	for (int i = 0; i < fileName.size(); i++)//填充报文数据段
		fileMessage.data[i] = fileName[i];
	fileMessage.data[fileName.size()] = '\0';
	fileMessage.setCheck();
	if (sendSegment(fileMessage) == true) 
		cout << "客户端成功发送名为 " << fileName << " 的文件，其大小是：" << fileSize << " 字节" << endl;
	else {
		cout << "文件 " << fileName << " 发送失败！" << endl;
		return false;
	}

	// 再发送文件具体内容：
	// 计算段长度：
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
			cout << "客户端成功发送seq=" << sendMessage.seqNum << "的报文段！" << endl;
		else {
			cout << " 第" << i << "个最大报文段发送失败！" << endl;
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
			cout << "客户端成功发送seq=" << sendMessage.seqNum << "的报文段！" << endl;
		else {
			cout << "余下的报文段发送失败！" << endl;
			return false;
		}
	}
	clock_t endTime = clock();
	cout << "文件" << fileName << "的总传输时间为：" << (endTime - startTime) / CLOCKS_PER_SEC << "s" << endl;
	cout << "传输过程的吞吐率为:" << ((float)fileSize) / ((endTime - startTime) / CLOCKS_PER_SEC) << "byte/s" << endl << endl;
	return true;
}
