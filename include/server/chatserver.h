#ifndef ChatServer_H
#define ChatServer_H

#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
using namespace muduo;
using namespace muduo::net;
using std::string;
/* 基于muduo网络库开发服务器程序
1. 组合TcpServer对象
2. 创建EventLoop事件循环对象指针
3. 明确TcpServer构造函数需要的参数->编写ChatServer构造函数

*/

class ChatServer
{
public:
    ChatServer(EventLoop *loop,
               const InetAddress &listenAddr,
               const string &nameArg);
    void start(); // 启动服务

private:
    void onConnection(const TcpConnectionPtr &);
    void onMessage(const TcpConnectionPtr &,
                   Buffer *,
                   Timestamp);

private:
    TcpServer _server;
    EventLoop *_loop; // epoll
};

#endif