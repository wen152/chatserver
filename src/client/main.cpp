#include <iostream>
#include <vector>
#include <thread>
#include <unordered_map>
#include <string>
#include <functional>
#include <chrono>
#include <semaphore.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <atomic>
#include <unistd.h>
#include "user.h"
#include "group.h"
#include "common.hpp"
#include "json.hpp"
using json = nlohmann::json;
using std::string;
using std::vector;
/*
    不需要高并发 普通socket TCP 编程
    主线程 send msg 用户不输入发送的消息则阻塞
    子线程 recv msg 该线程只启动一次
    _thread.detach()
*/

// 需要的全局变量
User currentUser;                   // 当前用户信息
vector<User> currentUserFriendList; // 好友列表
vector<Group> currentUserGroupList; // 群组列表信息
atomic_bool isLoginSuccess{false};  // 基于CAS的原子量 -> 线程安全
bool isMainMenuRunning = false;
sem_t rwsem; // 读写线程之间的通信(同步)

// 函数声明
string getCurrentTime();
void mainMenu(int);
void showCurrentUserData(int fd = 0, string str = "");
void doLoginResponse(json &);
void doRegResponse(json &);
void readTaskHandler(int);

// 业务函数声明
void help(int fd = 0, string str = "");
void chat(int, string);
void addfriend(int, string);
void creategroup(int, string);
void addgroup(int, string);
void groupchat(int, string);
void loginout(int, string);

// 系统支持的客户端命令列表
std::unordered_map<string, string> commandMap = {
    {"help\t", "\"show all commands\"\t\t -> format -> help"},
    {"chat\t", "\"one to one chat\"\t\t -> format -> chat:friendid:message"},
    {"addfriend", "\"add friend\"\t\t\t -> format -> addfriend:friendid"},
    {"creategroup", "\"create group\"\t\t\t -> format -> creategroup:groupname:groupdesc"},
    {"addgroup", "\"add group\"\t\t\t -> format -> addgroup:groupid"},
    {"groupchat", "\"group chat\"\t\t\t -> format -> groupchat:groupid:message"},
    {"loginout", "\"login out\"\t\t\t -> format -> loginout"}};
//{"showUserData", "\"show current user data\"\t -> format -> show"}};

// 注册系统支持的客户端命令处理 int -> socketfd
std::unordered_map<string, function<void(int, string)>>
    commandHandlerMap = {
        {"help", help},
        {"chat", chat},
        {"addfriend", addfriend},
        {"creategroup", creategroup},
        {"addgroup", addgroup},
        {"groupchat", groupchat},
        {"loginout", loginout}};
//{"show", showCurrentUserData}};

int main(int argc, char *argv[])
{
    // 输入IP port
    if (argc != 3)
    {
        cerr << "arguments invalid! example: ./ChatClient 127.0.0.1 8000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    // 填写client需要连接的server信息ip+port
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd == -1)
    {
        cerr << "socket create error" << endl;
        exit(-1);
    }

    sockaddr_in _server;
    memset(&_server, 0, sizeof(sockaddr_in));
    _server.sin_family = AF_INET; // 协议族
    _server.sin_port = htons(port);
    _server.sin_addr.s_addr = inet_addr(ip);

    if (connect(clientfd, (sockaddr *)&_server, sizeof(sockaddr_in)) == -1)
    {
        cerr << "connect server error" << endl;
        close(clientfd);
        exit(-1);
    }

    // 初始化读写线程通信用的信号量
    sem_init(&rwsem, 0, 0);

    // 连接服务器成功，启动接收子线程
    std::thread readTask(readTaskHandler, clientfd);
    readTask.detach();

    // main程序负责接收用户输入,发送消息
    for (;;)
    {
        // 显示首页面菜单 登录、注册、退出
        cout << "========================" << endl;
        cout << "1. login" << endl;
        cout << "2. register" << endl;
        cout << "3. quit" << endl;
        cout << "========================" << endl;
        int choice = 0;
        std::cin >> choice;
        if (cin.fail())
        {
            cin.clear();                                         // 清除错误标志
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // 丢弃这一行剩余输入
            cerr << "请输入合法的choice" << endl;
            continue;
        }
        std::cin.get(); // 防止回车被变量读入
        switch (choice)
        {
        case 1:
        {
            // 初始化
            currentUserFriendList.clear();
            currentUserGroupList.clear();
            // login成功后
            int userid = 0;
            char pwd[50] = {0};
            cout << "userid:";
            cin >> userid;
            if (cin.fail())
            {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cerr << "请输入合法的数字 userid!" << endl;
                break;
            }
            cin.get(); // 读掉缓冲区残留的回车
            cout << "password:";
            cin.getline(pwd, 50);
            json js;
            js["msgid"] = LOGIN_MSG;
            js["id"] = userid;
            js["password"] = pwd;
            string request = js.dump();
            isLoginSuccess = false;
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            if (len == -1)
                cerr << "send login msg error:" << request << endl;
            sem_wait(&rwsem);
            if (isLoginSuccess)
            {
                isMainMenuRunning = true;
                mainMenu(clientfd);
            }
        }
        break;
        case 2: // 处理register业务
        {
            char name[50] = {0};
            char pwd[50] = {0};
            cout << "username:";
            cin.getline(name, 50);
            cout << "password:";
            cin.getline(pwd, 50);
            json js;
            js["msgid"] = REGISTER_MSG;
            js["name"] = name;
            js["password"] = pwd;
            string request = js.dump();
            // +1 是为了发送字符串末尾的 \0 终止符
            // 阻塞发送
            int len = send(clientfd, request.c_str(), strlen(request.c_str()) + 1, 0);
            // cout << "len : " << len << endl;
            if (len == -1)
                cerr << "send register msg error:" << request << endl;
            sem_wait(&rwsem);
        }
        break;
        case 3: // quit业务
        {
            close(clientfd);
            sem_destroy(&rwsem);
            exit(0);
        }
        default:
            cerr << "Invalid input!" << endl;
            break;
        }
    }
    return 0;
}

void mainMenu(int fd)
{
    help();
    char buffer[1024] = {0};
    while (isMainMenuRunning) //
    {
        cin.getline(buffer, 1024);
        string commandbuf(buffer);
        string command;                 // 存储命令
        int idx = commandbuf.find(":"); // 返回下标
        if (idx == string::npos)
            command = commandbuf;
        else
            command = commandbuf.substr(0, idx); // 第一个:前面的部分为command->回调函数
        // cout << command << endl;
        auto it = commandHandlerMap.find(command);
        if (it == commandHandlerMap.end())
        {
            cerr << "invalid input command!" << endl;
            continue;
        }
        // cout << commandbuf.substr(idx + 1, commandbuf.size() - idx) << endl;
        // 调用相应命令的事件处理回调,mainMenu对修改封闭,添加新功能不需要修改该函数
        it->second(fd, commandbuf.substr(idx + 1, commandbuf.size() - idx)); // 调用命令处理方法
    }
}

// 显示当前登录成功用户的基本信息
void showCurrentUserData(int, string)
{
    cout << "======================login user======================" << endl;
    cout << "current login user => id:" << currentUser.getId() << " name:" << currentUser.getName() << endl;
    cout << "----------------------friend list---------------------" << endl;
    if (!currentUserFriendList.empty())
    {
        for (User &user : currentUserFriendList)
        {
            cout << user.getId() << " [" << user.getName() << "] " << user.getState() << endl;
        }
    }
    else
        cout << "NULL" << endl;
    cout << "----------------------group list----------------------" << endl;
    if (!currentUserGroupList.empty())
    {
        cout << "***" << endl;
        for (Group &group : currentUserGroupList)
        {
            cout << "群组[" << group.getId() << "] " << group.getName() << " " << group.getDesc() << endl;
            for (GroupUser &user : group.getUsers())
            {
                cout << user.getId() << " " << user.getName() << " " << user.getState()
                     << " " << user.getRole() << endl;
            }
        }
        cout << "***" << endl;
    }
    else
        cout << "NULL" << endl;
    cout << "======================================================" << endl;
}

void doLoginResponse(json &response)
{
    if (response["erron"].get<int>() == 0) // 登录成功
    {
        // 记录当前用户的id和name
        currentUser.setId(response["id"].get<int>());
        currentUser.setName(response["name"]);

        // 记录当前用户的好友列表信息chat
        if (response.contains("friend"))
        {
            vector<string> vec = response["friend"];
            for (string &str : vec)
            {
                json js = json::parse(str);
                User _friend;
                _friend.setId(js["friend_id"]);
                _friend.setName(js["friend_name"]);
                _friend.setState(js["friend_state"]);
                currentUserFriendList.push_back(_friend);
            }
        }

        // 记录当前用户的群组列表信息
        if (response.contains("groups"))
        {
            vector<string> vec = response["groups"];
            for (auto &str : vec)
            {
                json grpjs = json::parse(str);
                Group _group;
                _group.setId(grpjs["groupid"].get<int>());
                _group.setName(grpjs["groupname"]);
                _group.setDesc(grpjs["groupdesc"]);
                vector<string> _groupuserjs = grpjs["groupuser"];
                for (string &str : _groupuserjs)
                {
                    json js = json::parse(str);
                    GroupUser _groupuser;
                    _groupuser.setId(js["id"].get<int>());
                    _groupuser.setName(js["name"]);
                    _groupuser.setState(js["state"]);
                    _groupuser.setRole(js["role"]);
                    _group.getUsers().push_back(_groupuser);
                }
                currentUserGroupList.push_back(_group);
            }
        }

        // 显示登录用户的基本信息
        showCurrentUserData();

        // 显示当前用户的离线消息  个人聊天信息或者群组消息
        if (response.contains("offlinemsg"))
        {
            vector<string> vec = response["offlinemsg"];
            for (string &str : vec)
            {
                // time + [id] + name + " said: " + xxx
                json js = json::parse(str);
                if (js["msgid"] == ONE_CHAT_MSG)
                {
                    cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["from"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
                else if (js["msgid"] == GROUP_CHAT_MSG)
                {
                    cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["from"].get<string>()
                         << " said: " << js["msg"].get<string>() << endl;
                }
            }
        }
        isLoginSuccess = true;
    }
    else
    {
        cerr << response["errmsg"] << endl;
        isLoginSuccess = false;
    }
}

void doRegResponse(json &response)
{
    if (response["erron"] == 0)
        cout << "name register success, userid is " << response["id"]
             << ", do not forget it!" << endl;
    else
        cerr << "register error!" << endl;
}

void help(int, string)
{
    cout << "show command list >>>" << endl;
    for (auto &_command : commandMap)
        cout << _command.first << "\t" << _command.second << endl;
    cout << endl;
}

void chat(int fd, string str)
{
    //{"msgid":6,"id":22,"from":"zcw","toid":13,"msg":"hello!"}
    int idx = str.find(":");
    if (idx == string::npos)
    {
        cerr << "OneChat : invalid input command!" << endl;
        return;
    }
    int toid = atoi(str.substr(0, idx).c_str()); // 不只是发给好友
    string msg = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = ONE_CHAT_MSG;
    js["id"] = currentUser.getId();
    js["from"] = currentUser.getName();
    js["toid"] = toid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send onechat msg error -> " << buffer << endl;
}

void addfriend(int fd, string str)
{
    if (str == "")
    {
        cerr << "do you input frienid ?" << endl;
        return;
    }
    int friendid = atoi(str.c_str());
    json js;
    js["msgid"] = ADD_FRIEND_MSG;
    js["id"] = currentUser.getId();
    js["friendid"] = friendid;
    string buffer = js.dump();
    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send addfriend msg error -> " << buffer << endl;
}

void creategroup(int fd, string str)
{
    int idx = str.find(":");
    if (idx == string::npos)
    {
        cerr << "Creategroup : invalid input command!" << endl;
        return;
    }
    string groupname = str.substr(0, idx);
    string groupdesc = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = CREATE_GROUP_MSG;
    js["id"] = currentUser.getId();
    js["groupname"] = groupname;
    js["groupdesc"] = groupdesc;
    string buffer = js.dump();
    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send createfriend msg error -> " << buffer << endl;
}

void addgroup(int fd, string str)
{
    json js;
    js["msgid"] = ADD_GROUP_MSG;
    js["id"] = currentUser.getId();
    js["groupid"] = atoi(str.c_str());
    string buffer = js.dump();
    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send addgroup msg error -> " << buffer << endl;
}

void groupchat(int fd, string str)
{
    //{"msgid":12,"id":22,"from":"zcw","groupid":1,"msg":"everybody hello?"}
    int idx = str.find(":");
    if (idx == string::npos)
    {
        cerr << "GroupChat : invalid input command!" << endl;
        return;
    }
    int groupid = atoi(str.substr(0, idx).c_str());
    string msg = str.substr(idx + 1, str.size() - idx);
    json js;
    js["msgid"] = GROUP_CHAT_MSG;
    js["id"] = currentUser.getId();
    js["from"] = currentUser.getName();
    js["groupid"] = groupid;
    js["msg"] = msg;
    js["time"] = getCurrentTime();
    string buffer = js.dump();
    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send groupchat msg error -> " << buffer << endl;
}

// 注销
void loginout(int fd, string str)
{
    json js;
    js["msgid"] = LOGIN_OUT_MSG;
    js["id"] = currentUser.getId();
    string buffer = js.dump();
    int len = send(fd, buffer.c_str(), strlen(buffer.c_str()) + 1, 0);
    if (len == -1)
        cerr << "send loginout msg error -> " << buffer << endl;
    else
        isMainMenuRunning = false;
}

// 获取系统时间（聊天信息需要添加时间信息）
string getCurrentTime()
{
    auto t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct tm *ptm = localtime(&t);
    char sendtime[60] = {0};
    sprintf(sendtime, "%d-%02d-%02d %02d:%02d:%02d", (int)ptm->tm_year + 1900, (int)ptm->tm_mon + 1, (int)ptm->tm_mday,
            (int)ptm->tm_hour, (int)ptm->tm_min, (int)ptm->tm_sec);
    return string(sendtime);
}

void readTaskHandler(int fd)
{
    for (;;)
    {
        char buffer[4096] = {0};
        int len = recv(fd, buffer, 4096, 0); // 阻塞接收
        // cout << buffer << endl;
        if (len == -1 || len == 0)
        {
            close(fd);
            exit(-1);
        }

        // 接收ChatServer转发的数据，反序列化生成json数据对象
        json js = json::parse(buffer);
        int msgtype = js["msgid"].get<int>();
        // cout << "msgtype" << msgtype << endl;
        if (msgtype == ONE_CHAT_MSG)
        {
            cout << js["time"].get<string>() << " [" << js["id"] << "]" << js["from"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (msgtype == ONE_CHAT_ACK)
        {
            if (js["erron"].get<int>() == 0)
                cout << js["msg"] << endl;
            else
                cout << js["errmsg"] << endl;
            continue;
        }

        if (msgtype == GROUP_CHAT_MSG)
        {
            cout << "群消息[" << js["groupid"] << "]:" << js["time"].get<string>() << " [" << js["id"] << "]" << js["from"].get<string>()
                 << " said: " << js["msg"].get<string>() << endl;
            continue;
        }

        if (msgtype == GROUP_CHAT_ACK)
        {
            if (js["erron"].get<int>() == 0)
                cout << js["msg"] << endl;
            else
                cout << js["errmsg"] << endl;
            continue;
        }

        if (msgtype == LOGIN_ACK)
        {
            doLoginResponse(js); // 处理登录响应的业务逻辑
            sem_post(&rwsem);    // 通知主线程，登录结果处理完成
            continue;
        }

        if (msgtype == REGISTER_ACK)
        {
            doRegResponse(js);
            sem_post(&rwsem); // 通知主线程，注册结果处理完成
            continue;
        }

        if (msgtype == ADD_FRIEND_ACK)
        {
            if (js["erron"].get<int>() == 0)
                cout << "add friend successfully!" << endl;
            else
                cout << "add friend failed!" << endl;
            continue;
        }

        if (msgtype == ADD_GROUP_ACK)
        {
            if (js["erron"].get<int>() == 0)
                cout << "add group successfully!" << endl;
            else
                cout << "add group failed!" << endl;
            continue;
        }

        if (msgtype == CREATE_GROUP_ACK)
        {
            if (js["erron"].get<int>() == 0)
                cout << js["msg"] << endl;
            else
                cout << js["erromsg"] << endl;
            continue;
        }
    }
}