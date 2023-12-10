#include "segment.h"
Message::Message()
{
	// ��ʼѡ�����к�0
	seqNum = 0;
	ackNum = 0;
	size = 0;
	flag = 0;
	checkNum = 0;
	memset(&data, 0, MSS);
}
void Message::setPort(unsigned short src, unsigned short dst)
{
	this->srcPort = src;
	this->dstPort = dst;
}
bool Message::getCheck()
{
	unsigned int sum = 0; //У��ͣ�16λ����1�򷵻�true
	// �ѵ�ǰ�Ľṹ�嵱��һ��16λ�����飬��������������飬ÿ����16λ���������
	unsigned short* dataAs16bit = (unsigned short*)this;
	// ѭ������Ϊ��λ��/16�������ֽ���/2
	for (int i = 0; i < sizeof(*this) / 2; i++) {
		sum += *dataAs16bit++; //��Ӻ�ָ������
		// �����Ӻ��32λ�����ĸ�16λ��Ϊ0
		if (sum & 0xFFFF0000) 
		{
			sum &= 0xFFFF; // �Ѹ�16λ��0
			sum++; // ����ĩβ+1
		}
	}
	if ((sum & 0xFFFF) == 0xFFFF) // ���16λ����1����ô���ݱ�û�з�������
		return true;
	return false;
}
void Message::setCheck()
{
	this->checkNum = 0; // У����ֶ���0
	unsigned int sum = 0;
	// �ѵ�ǰ�Ľṹ�嵱��һ��16λ�����飬��������������飬ÿ����16λ���������
	unsigned short* dataAs16bit = (unsigned short*)this;
	// ѭ������Ϊ��λ��/16�������ֽ���/2
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *dataAs16bit++;
		if (sum & 0xFFFF0000)
		{
			sum &= 0xFFFF;
			sum++;
		}
	}
	this->checkNum = ~(sum & 0xFFFF); // ������ȡ������У����ֶ�
}