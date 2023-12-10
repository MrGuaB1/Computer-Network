#pragma once
#pragma pack(1)
#include <iostream>
using namespace std;
const int SERVER_PORT = 10000, CLIENT_PORT = 30000;
const int MAX_WAIT_TIME = 500; // 超时时间
const int MSS = 10000; // 最大段长度
const int LONGEST = 200000000; // 最大文件大小


// 定义几个标志位：
const unsigned short SYN = 0x1;  // 建立连接
const unsigned short ACK = 0x2;  // 响应
const unsigned short FIN = 0x4;  // 释放连接
struct Message
{
	// 数据报首部
	char srcIP[16], dstIP[16];
	unsigned short srcPort, dstPort;
	unsigned int seqNum; //序列号 seq
	unsigned int ackNum; //确认号 ack
	unsigned int size; //数据大小
	unsigned short flag; //标志位
	unsigned short checkNum; //校验位
	unsigned int recv_size; // 接收窗口通告
	unsigned int base; // 接收窗口开始位置

	// 数据报内容，字节流报文
	char data[MSS];

	Message();
	bool getCheck(); // 求校验和，如果结果的16位整数全为1则返回true
	void setCheck(); // 将数据报补齐为16的整数倍，并且按位取反求和放入校验位
	void setPort(unsigned short src, unsigned short dst); // 设置源端口和目的端口
	void set_SYN() { flag += SYN; }
	void set_ACK() { flag += ACK; }
	void set_FIN() { flag += FIN; }
	void set_size(int size) { recv_size = size; }
	void set_base(int base) { this->base = base; }
};