#ifndef CHATSERVICE_H
#define CHATSERVER_H

#include <muduo/net/TcpConnection.h>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <redis.h>
#include "usermodel.h"
#include "offlinemessagemodel.h"
#include "friendmodel.h"
#include "groupmodel.h"
#include "json.hpp"
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

using MsgHandler = std::function<void(const TcpConnectionPtr &, json &, Timestamp)>;

// 聊天服务器服务类
class ChatService
{
public:
    // 单例
    static ChatService *getInstance();
    // 获取消息对应的处理函数对象
    MsgHandler getHandler(int);
    // 处理登录业务
    void _login(const TcpConnectionPtr &, json &, Timestamp);
    // 处理注销业务
    void _loginout(const TcpConnectionPtr &, json &, Timestamp);
    // 处理注册业务
    void _register(const TcpConnectionPtr &, json &, Timestamp);
    // 一对一聊天业务
    void _oneChat(const TcpConnectionPtr &, json &, Timestamp);
    // 添加好友
    void _addFriend(const TcpConnectionPtr &, json &, Timestamp);
    // 创建群组业务
    void _createGroup(const TcpConnectionPtr &, json &, Timestamp);
    // 加入群组业务
    void _addGroup(const TcpConnectionPtr &, json &, Timestamp);
    // 群组聊天业务
    void _groupChat(const TcpConnectionPtr &, json &, Timestamp);
    // 处理客户端异常退出
    void _clientCloseException(const TcpConnectionPtr &);
    // 服务器异常，业务重置方法
    void _reset();

    void handleRedisSubscribeMessage(int, string);

private:
    ChatService(); // 构造私有化
    ChatService(const ChatService &) = delete;
    ChatService operator=(const ChatService &) = delete;

private:
    // 存储消息id和其对应的业务处理方法
    std::unordered_map<int, MsgHandler> _msgHandlerMap;
    // 存储在线用户的通信连接 //连接的智能指针
    std::unordered_map<int, TcpConnectionPtr> _userConnMap;
    UserModel _userModel;
    OfflineMessageModel _offlineMessageModel;
    FriendModel _friendModel;
    GroupModel _groupModel;
    std::mutex _connMutex;
    Redis _redis;
};

#endif