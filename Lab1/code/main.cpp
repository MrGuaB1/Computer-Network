#include <iostream>
#include "client.h"
#include "server.h"
#pragma warning(disable:4996)
using namespace std;
int main()
{
    //system("chcp 65001"); //在Clion中解决命令行显示中文乱码
    int which;
    cout << "Press 0 to be server,and press 1 to be client： ";
    cin >> which;
    if (which == 0)
        server();
    else if (which == 1)
        client();
    return 0;
}
