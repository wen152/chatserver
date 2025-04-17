#include <muduo/base/Logging.h>
#include "usermodel.h"
#include "common.hpp"
#include "db.h"

// User表的增加方法
bool UserModel::insert(User &user)
{
    // 1.组装sql语句
    char sql[1024] = {0};
    // 格式化字符串 调用 c_str() 的作用是将 std::string 转换成 C 风格的字符串，这样才能被 %s 正确识别和输出。
    sprintf(sql, "insert into user(name,password,state) values('%s','%s','%s')",
            user.getName().c_str(), user.getPassword().c_str(), user.getState().c_str());
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的用户数据生成的主键id
            user.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

User UserModel::query(int id)
{
    char sql[1024] = {0};
    sprintf(sql, "select * from user where id = %d", id);
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res); // char**
            if (row != nullptr)
            {
                User _user(atoi(row[0]), row[1], row[2], row[3]);
                mysql_free_result(res); // 释放查询到的数据
                return _user;
            }
        }
    }
    return User(); // 返回默认构造的User用户 User user();会被认作是函数声明
}

bool UserModel::updateState(const User &user)
{
    char sql[1024] = {0};
    // LOG_INFO << user.getState();
    sprintf(sql, "update user set state='%s' where id = %d", user.getState().c_str(), user.getId());
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
            return true;
    }
    return false;
}

void UserModel::resetState()
{
    char sql[1024] = {0};
    sprintf(sql, "update user set state='offline' where state = 'online'");
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}
