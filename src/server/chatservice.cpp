#include <muduo/base/Logging.h>
#include <functional>
#include <memory>
#include "chatservice.h"
#include "common.hpp"
#include "user.h"
using namespace muduo;
// 使用muduo库的日志(如何echo到log文件) LOG_ERROR

// 获取单例对象的接口函数
ChatService *ChatService::getInstance()
{ // 类外实现不需要加static
    static ChatService _chatservice;
    return &_chatservice;
}

// 注册消息以及对应的Handler回调操作
ChatService::ChatService()
{
    _msgHandlerMap.insert({LOGIN_MSG, bind(&ChatService::_login, this, _1, _2, _3)});
    _msgHandlerMap.insert({LOGIN_OUT_MSG, bind(&ChatService::_loginout, this, _1, _2, _3)});
    _msgHandlerMap.insert({REGISTER_MSG, bind(&ChatService::_register, this, _1, _2, _3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, bind(&ChatService::_oneChat, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG, bind(&ChatService::_addFriend, this, _1, _2, _3)});
    _msgHandlerMap.insert({CREATE_GROUP_MSG, bind(&ChatService::_createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, bind(&ChatService::_addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, bind(&ChatService::_groupChat, this, _1, _2, _3)});
    // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

// 获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    auto it = _msgHandlerMap.find(msgid);
    // 记录错误日志
    if (it == _msgHandlerMap.end())
        LOG_ERROR << "msgid: " << msgid << " cannot find handler!";
    return _msgHandlerMap[msgid];
}

// 处理登录业务(回调函数)
void ChatService::_login(const TcpConnectionPtr &conn, json &js, Timestamp _time)
{
    // LOG_INFO << "do login service";
    int id = js["id"].get<int>(); //.get<int>()
    string pwd = js["password"];
    User _user = _userModel.query(id);
    if (_user.getId() == id && _user.getPassword() == pwd)
    {
        if (_user.getState() == ONLINE)
        {
            LOG_ERROR << "The user has logined!";
            json response;
            response["msgid"] = LOGIN_ACK;
            response["erron"] = 1;
            response["errmsg"] = "this account is using, input another!";
            conn->send(response.dump());
        }
        else
        {
            {
                // 登录成功,记录用户连接信息 保证线程安全
                std::unique_lock<std::mutex> lock(_connMutex);
                _userConnMap.insert({id, conn});
            }

            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id);

            // 登录状态 -> online
            _user.setState(ONLINE);
            _userModel.updateState(_user);
            LOG_INFO << "The user login successfully!";
            json response;
            response["msgid"] = LOGIN_ACK;
            response["erron"] = 0;
            response["id"] = _user.getId();
            response["name"] = _user.getName();

            // 查询该用户是否有离线消息
            std::vector<string> _offlineMsg = _offlineMessageModel.query(id);
            if (!_offlineMsg.empty())
            {
                response["offlinemsg"] = _offlineMsg; // 直接序列化一个vector容器/map容器
                _offlineMessageModel.remove(id);
            }

            // 查询该用户的好友信息并返回
            std::vector<User> _uservec = _friendModel.query(id);
            if (!_uservec.empty())
            {
                vector<string> vec;
                for (auto &user : _uservec)
                {
                    json js;
                    js["friend_id"] = user.getId();
                    js["friend_name"] = user.getName();
                    js["friend_state"] = user.getState();
                    vec.push_back(js.dump());
                }
                response["friend"] = vec;
            }

            // 查询用户的群组信息
            vector<Group> _groupvec = _groupModel.queryGroups(id);
            if (!_groupvec.empty())
            {
                // groups:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> vec;
                for (auto &_group : _groupvec)
                {
                    json grpjson;
                    grpjson["groupid"] = _group.getId();
                    grpjson["groupname"] = _group.getName();
                    grpjson["groupdesc"] = _group.getDesc();
                    vector<string> _groupuserjs;
                    for (GroupUser &_user : _group.getUsers())
                    {
                        json js;
                        js["id"] = _user.getId();
                        js["name"] = _user.getName();
                        js["state"] = _user.getState();
                        js["role"] = _user.getRole();
                        _groupuserjs.push_back(js.dump());
                    }
                    grpjson["groupuser"] = _groupuserjs;
                    vec.push_back(grpjson.dump());
                }
                response["groups"] = vec;
            }
            conn->send(response.dump());
        }
    }
    else
    {
        LOG_ERROR << "The user has not registered!";
        json response;
        response["msgid"] = LOGIN_ACK;
        response["erron"] = 2;
        response["errmsg"] = "id or password is invalid!";
        conn->send(response.dump());
    }
}

// 处理注销业务
void ChatService::_loginout(const TcpConnectionPtr &conn, json &js, Timestamp _time)
{
    int id = js["id"].get<int>();
    {
        unique_lock<mutex> lock(_connMutex);
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
            _userConnMap.erase(it);
    }

    // redis取消订阅通道
    _redis.unsubscribe(id);

    // 下线更新状态
    User user(id, "", "", OFFLINE);
    _userModel.updateState(user);
}

// 处理注册业务 ORM框架 业务层操作的都是对象 DAO 封装数据库操作
void ChatService::_register(const TcpConnectionPtr &conn, json &js, Timestamp _time)
{
    // LOG_INFO << "do register service";

    // 操作的都是User表项对象 根据name password注册 插入user表中
    // 业务模块和数据库操作解耦
    string name = js["name"];
    string pwd = js["password"];
    User user; // 主键在mysql数据库操作时赋值
    user.setName(name);
    user.setPassword(pwd);

    bool flag = _userModel.insert(user); // 如果name相同会返回false
    if (flag)
    {
        // 注册成功
        json response;
        response["msgid"] = REGISTER_ACK;
        response["erron"] = 0;
        response["id"] = user.getId();
        conn->send(response.dump()); // 序列化响应发回
    }
    else
    {
        // 注册失败
        json response;
        response["msgid"] = REGISTER_ACK;
        response["erron"] = 1;
        conn->send(response.dump());
    }
}

// 一对一聊天服务
void ChatService::_oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["toid"].get<int>();
    json response;
    // toid在线,转发消息 服务器主动推送消息给toid用户 在本服务器上
    {
        std::unique_lock<std::mutex> lock(_connMutex);
        if (_userConnMap.find(toid) != _userConnMap.end())
        {
            _userConnMap[toid]->send(js.dump());
            LOG_INFO << "onechat send message successfully!";
            response["receiverstate"] = ONLINE;
            response["msg"] = "send message successfully! and online";
            conn->send(response.dump());
            return;
        }
    }
    // 检查是否有这个人
    User user = _userModel.query(toid);
    if (user.getId() == -1)
    {
        LOG_ERROR << "find receiver failed!";
        response["msgid"] = ONE_CHAT_ACK;
        response["erron"] = 1;
        response["errmsg"] = "can not find this user, send failed!";
        conn->send(response.dump());
        return;
    }
    response["msgid"] = ONE_CHAT_ACK;
    response["erron"] = 0;

    // 可能在redis中
    if (user.getState() == ONLINE)
    {
        _redis.publish(toid, js.dump());
        LOG_INFO << "receiver state: online";
        response["receiverstate"] = ONLINE;
        response["msg"] = "send message successfully! and online";
        conn->send(response.dump());
        return;
    }

    // toid不在线,存储离线消息
    _offlineMessageModel.insert(toid, js.dump());
    LOG_INFO << "receiver state: offline";
    response["receiverstate"] = OFFLINE;
    response["msg"] = "send message successfully! but offline";
    conn->send(response.dump());
}

// 添加好友业务 msgid id friendid
void ChatService::_addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();
    json response;
    response["msgid"] = ADD_FRIEND_ACK;
    if (userid != friendid)
    {
        // 存储好友信息
        if (_friendModel.insert(userid, friendid))
        {
            response["erron"] = 0;
            response["friendid"] = friendid;
            response["msg"] = "add friend successfully!";
            LOG_INFO << "add friend successfully!";
        }
        else
        {
            response["erron"] = 1;
            response["errmsg"] = "add friend failed!";
            LOG_ERROR << "add friend failed!";
        }
    }
    else
    {
        response["erron"] = 1;
        response["errmsg"] = "friendid = userid is invalid!";
        LOG_ERROR << "friendid = userid is invalid!";
    }
    conn->send(response.dump());
}

// 创建群组业务
void ChatService::_createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    // 创建人信息
    int id = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];
    Group _group(-1, name, desc);
    json response;
    response["msgid"] = CREATE_GROUP_ACK;
    if (_groupModel.createGroup(_group))
    {
        response["erron"] = 0;
        response["groupname"] = name;
        response["groupdesc"] = desc;
        response["groupid"] = _group.getId();
        response["msg"] = "create group successfully!";
        LOG_INFO << "create group successfully!";
        _groupModel.addGroup(id, _group.getId(), CREATER);
    }
    else
    {
        response["erron"] = 1;
        response["errmsg"] = "create group fail!";
        LOG_ERROR << "create group failed!";
    }
    conn->send(response.dump());
}

// 加入群组业务
void ChatService::_addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    json response;
    response["msgid"] = ADD_GROUP_ACK;
    if (_groupModel.addGroup(userid, groupid, NORMAL))
    {
        response["erron"] = 0;
        response["groupid"] = groupid;
        response["msg"] = "add group successfully!";
        LOG_INFO << "add group successfully!";
    }
    else
    {
        response["erron"] = 1;
        response["errmsg"] = "add group failed!";
        LOG_ERROR << "add group failed!";
    }
    conn->send(response.dump());
}

// 群组聊天业务
void ChatService::_groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    // 不在这个群组里
    json response;
    response["msgid"] = GROUP_CHAT_ACK;
    if (!_groupModel.isGroupUser(userid, groupid))
    {
        response["erron"] = 1;
        response["errmsg"] = "you are not in this group!";
        conn->send(response.dump());
        return;
    }
    vector<int> _groupchatusers = _groupModel.queryGroupUsers(userid, groupid);
    if (!_groupchatusers.empty())
    {
        unique_lock<mutex> lock(_connMutex);
        for (auto toid : _groupchatusers)
        {
            auto it = _userConnMap.find(toid);
            if (it != _userConnMap.end())
            {
                // toid在线
                it->second->send(js.dump());
            }
            else
            {
                // redis如何修改
                User user = _userModel.query(toid);
                if (user.getState() == ONLINE)
                {
                    _redis.publish(toid, js.dump());
                    LOG_INFO << "group chat send message successfully!";
                }
                else
                {
                    // toid不在线
                    _offlineMessageModel.insert(toid, js.dump());
                }
            }
        }
        response["erron"] = 0;
        response["groupid"] = groupid;
        response["msg"] = "group chat successfully!";
        LOG_INFO << "group chat successfully!";
    }
    else
    {
        response["erron"] = 1;
        response["errmsg"] = "group user is empty!";
        conn->send(response.dump());
        return;
    }
    conn->send(response.dump());
}

// 处理客户端异常退出
void ChatService::_clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        std::unique_lock<std::mutex> lock(_connMutex);
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
            if (it->second == conn)
            {
                user.setId(it->first);
                LOG_INFO << user.getId();
                _userConnMap.erase(it); // 从map表删除用户的链接信息
                break;
            }
        }
    }
    if (user.getId() != -1)
    {
        _redis.unsubscribe(user.getId());
        // 下线更新状态
        user.setState(OFFLINE);
        _userModel.updateState(user);
    }
}

void ChatService::_reset()
{
    // 把online状态的用户，设置成offline
    _userModel.resetState();
}

// 订阅了 id1 通道的服务器接收到消息后，调用 handleRedisSubscribeMessage
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    unique_lock<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    // 发送过来的时候id1可能会因为各种问题突然下线
    if (it != _userConnMap.end())
    {
        // 在线用户
        it->second->send(msg);
        return;
    }
    else
    {
        // 离线用户
        _offlineMessageModel.insert(userid, msg);
    }
}