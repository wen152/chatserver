#ifndef USERMODEL_H
#define USERMODEL_H

#include "user.h"

// User表的数据操作类
class UserModel
{
public:
    // User表的增加方法
    bool insert(User &user);
    // 根据用户id查询用户信息
    User query(int id);
    // User表更新用户登录状态
    bool updateState(const User &user);
    // 重置用户的状态信息
    void resetState();

private:
};

#endif