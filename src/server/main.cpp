#include <iostream>
#include <signal.h>
#include "chatserver.h"
#include "chatservice.h"
#include "db.h"
using namespace muduo::net;

// 处理服务器ctrl+c结束后，重置user的状态信息
void resetHandler(int)
{
    ChatService::getInstance()->_reset();
    exit(0);
}

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }
    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);
    signal(SIGINT, resetHandler);
    EventLoop loop;
    InetAddress addr(ip, port);
    ChatServer chatserver(&loop, addr, "ChatServer");
    chatserver.start();
    loop.loop(); // epoll_wait以阻塞的方式等待新用户连接,已连接用户的读写事件
    return 0;
}