#ifndef OFFLINE_MESSAGE_MODEL_H
#define OFFLINE_MESSAGE_MODEL_H
#include <string>
#include <vector>
using std::string;
// 提供离线消息表的操作接口方法
class OfflineMessageModel
{
public:
    // 存储用户的离线消息
    void insert(int userid, string msg);

    // 删除用户的离线消息
    void remove(int userid);

    // 查询用户的离线消息
    std::vector<string> query(int userid);

private:
};

#endif