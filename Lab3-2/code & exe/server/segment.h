#pragma once
#pragma pack(1)
#include <iostream>
using namespace std;
const int SERVER_PORT = 10000, CLIENT_PORT = 30000;
const int MAX_WAIT_TIME = 1000; // ��ʱʱ��
const int MSS = 10000; // ���γ���

// lab3-2������
const int WINDOW_SIZE = 1; // ���մ��ڴ�С

// ���弸����־λ��
const unsigned short SYN = 0x1;  // ��������
const unsigned short ACK = 0x2;  // ��Ӧ
const unsigned short FIN = 0x4;  // �ͷ�����
struct Message
{
	// ���ݱ��ײ�
	char srcIP[16], dstIP[16];
	unsigned short srcPort, dstPort;
	unsigned int seqNum; //���к� seq
	unsigned int ackNum; //ȷ�Ϻ� ack
	unsigned int size; //���ݴ�С
	unsigned short flag; //��־λ
	unsigned short checkNum; //У��λ

	// ���ݱ����ݣ��ֽ�������
	char data[MSS];

	Message();
	bool getCheck(); // ��У��ͣ���������16λ����ȫΪ1�򷵻�true
	void setCheck(); // �����ݱ�����Ϊ16�������������Ұ�λȡ����ͷ���У��λ
	void setPort(unsigned short src, unsigned short dst); // ����Դ�˿ں�Ŀ�Ķ˿�
	void set_SYN() { flag += SYN; }
	void set_ACK() { flag += ACK; }
	void set_FIN() { flag += FIN; }
};