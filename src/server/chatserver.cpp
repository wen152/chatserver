#include <iostream>
#include <functional>
#include <string>
#include "chatserver.h"
#include "chatservice.h"
#include "json.hpp"
#include <muduo/base/Logging.h>
using namespace muduo;
using namespace std::placeholders;
using json = nlohmann::json;

// 初始化聊天服务器对象
ChatServer::ChatServer(EventLoop *loop,
                       const InetAddress &listenAddr,
                       const string &nameArg)
    : _server(loop, listenAddr, nameArg), _loop(loop)
{
    // 给服务器注册用户连接的创建和断开回调
    _server.setConnectionCallback(bind(&ChatServer::onConnection, this, _1));
    // 给服务器注册用户读写事件回调
    _server.setMessageCallback(bind(&ChatServer::onMessage, this, _1, _2, _3));
    // 设置线程数量 一个I/O线程 3个worker线程
    _server.setThreadNum(4);
}

void ChatServer::start()
{
    _server.start();
}

// 注册连接断开回调
// 上报连接相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr &conn)
{
    if (conn->connected())
        LOG_INFO << conn->peerAddress().toIpPort() << "->"
                 << conn->localAddress().toIpPort() << " - state::online";
    // 为什么不能else
    if (!conn->connected())
    {
        // 客户端断开连接
        ChatService::getInstance()->_clientCloseException(conn);
        LOG_INFO << conn->peerAddress().toIpPort() << "->"
                 << conn->localAddress().toIpPort() << " - state::offline";
        conn->shutdown();
    }
}

// 注册消息回调 多线程处理
// 上报读写事件相关信息的回调函数
void ChatServer::onMessage(const TcpConnectionPtr &conn, // 连接
                           Buffer *buffer,               // 缓冲区
                           Timestamp _time)              // 接收到数据的时间信息
{
    /*
    string buf = buffer->retrieveAllAsString();
    LOG_INFO << "recv data: " << buf << "time: " << _time.toFormattedString(false);
    conn->send(buf); // 回写
    */
    string buf = buffer->retrieveAllAsString();
    // 测试，添加json打印代码
    cout << buf << endl;
    // 数据反序列化
    json js = json::parse(buf);
    // 通过事件回调解构网络模块和业务模块
    // js["msgid"] -> 获取业务handler -> conn js time
    auto msgHandler = ChatService::getInstance()->getHandler(js["msgid"].get<int>()); // 将这个键对应的值强转成int型
    // 回调函数绑定好的事件处理器 来执行相应的业务操作
    msgHandler(conn, js, _time);
}
