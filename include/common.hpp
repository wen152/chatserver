#ifndef COMMON_HPP
#define COMMON_HPP

enum MsgType
{
    LOGIN_MSG = 1,    // 登录
    LOGIN_ACK,        // 登录响应
    LOGIN_OUT_MSG,    // 注销
    REGISTER_MSG,     // 注册 "msgid":,"name":,"password"
    REGISTER_ACK,     // 注册响应
    ONE_CHAT_MSG,     // 1对1聊天 "msgid":,"id":,"from":,"to":,"msg":
    ONE_CHAT_ACK,     //
    ADD_FRIEND_MSG,   // 添加好友
    ADD_FRIEND_ACK,   //
    CREATE_GROUP_MSG, // 创建群组
    CREATE_GROUP_ACK, //
    ADD_GROUP_MSG,    // 加入群组
    ADD_GROUP_ACK,    //
    GROUP_CHAT_MSG,   // 群聊天
    GROUP_CHAT_ACK,   //
};
#define OFFLINE "offline"
#define ONLINE "online"
#define CREATER "creator"
#define NORMAL "normal"

#endif