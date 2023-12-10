#include "segment.h"
Message::Message()
{
	// 初始选择序列号0
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
	unsigned int sum = 0; //校验和，16位都是1则返回true
	// 把当前的结构体当成一个16位的数组，后续遍历这个数组，每两个16位数进行求和
	unsigned short* dataAs16bit = (unsigned short*)this;
	// 循环次数为总位数/16，即总字节数/2
	for (int i = 0; i < sizeof(*this) / 2; i++) {
		sum += *dataAs16bit++; //相加后指针右移
		// 如果相加后的32位整数的高16位不为0
		if (sum & 0xFFFF0000) 
		{
			sum &= 0xFFFF; // 把高16位清0
			sum++; // 并在末尾+1
		}
	}
	if ((sum & 0xFFFF) == 0xFFFF) // 如果16位都是1，那么数据报没有发生错误
		return true;
	return false;
}
void Message::setCheck()
{
	this->checkNum = 0; // 校验和字段清0
	unsigned int sum = 0;
	// 把当前的结构体当成一个16位的数组，后续遍历这个数组，每两个16位数进行求和
	unsigned short* dataAs16bit = (unsigned short*)this;
	// 循环次数为总位数/16，即总字节数/2
	for (int i = 0; i < sizeof(*this) / 2; i++)
	{
		sum += *dataAs16bit++;
		if (sum & 0xFFFF0000)
		{
			sum &= 0xFFFF;
			sum++;
		}
	}
	this->checkNum = ~(sum & 0xFFFF); // 计算结果取反放入校验和字段
}