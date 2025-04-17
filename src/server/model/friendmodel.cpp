#include <muduo/base/Logging.h>
#include "friendmodel.h"
#include "db.h"

// 添加好友关系
bool FriendModel::insert(int userid, int friendid)
{
    char sql[1024] = {0};

    MySQL mysql;
    if (mysql.connect())
    {
        // 1. 检查好友关系是否已经存在
        sprintf(sql, "SELECT userid FROM friend WHERE userid = %d AND friendid=%d", userid, friendid);
        MYSQL_RES *res = mysql.query(sql);
        if (res == nullptr)
        {
            LOG_ERROR << "Query failed: " << mysql_error(mysql.getConnection());
            return false; // 查询失败
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row != nullptr)
        {
            mysql_free_result(res);
            LOG_INFO << "friend already exists: (" << userid << ", " << friendid << ")";
            return false;
        }
        mysql_free_result(res);

        // 2. 检查friendid是否存在
        sprintf(sql, "SELECT id FROM user WHERE id = %d", friendid);
        res = mysql.query(sql);
        if (res == nullptr)
        {
            LOG_ERROR << "Query failed: " << mysql_error(mysql.getConnection());
            return false; // 查询失败
        }
        row = mysql_fetch_row(res);
        if (row == nullptr)
        {
            mysql_free_result(res);
            LOG_INFO << "friend does not exist: " << friendid;
            return false; // 好友不存在
        }
        mysql_free_result(res); // 需要把上一个查询的结果“吃干净”再发起了新的命令,否则导致协议不同步（Commands out of sync）

        // 3. 插入friend表格
        sprintf(sql, "INSERT INTO friend VALUES(%d, %d)", userid, friendid);
        if (!mysql.update(sql))
        {
            LOG_ERROR << "Insert failed: " << mysql_error(mysql.getConnection());
            return false; // 插入失败
        }
        sprintf(sql, "INSERT INTO friend VALUES(%d, %d)", friendid, userid);
        if (!mysql.update(sql))
        {
            LOG_ERROR << "Insert failed: " << mysql_error(mysql.getConnection());
            return false; // 插入失败
        }
    }
    return true;
}

// 返回用户好友列表
std::vector<User> FriendModel::query(int userid)
{
    std::vector<User> _uservec;
    char sql[1024] = {0};
    // 多表查询 内联
    sprintf(sql, "select a.id,a.name,a.state from user a inner join friend b on b.friendid = a.id where b.userid = %d", userid);
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            // 每次调用mysql_fetch_row,它都会返回下一行的指针,直到没有更多行时,返回nullptr
            while ((row = mysql_fetch_row(res)) != nullptr) // 多行数据
            {
                User user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                _uservec.push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return _uservec;
}