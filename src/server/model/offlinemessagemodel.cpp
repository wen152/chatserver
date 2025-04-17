#include <muduo/base/Logging.h>
#include "offlinemessagemodel.h"
#include "db.h"

// 存储用户的离线消息
void OfflineMessageModel::insert(int userid, string msg)
{
    // json msg -> string msg 存储json字符串
    char sql[1024] = {0};
    sprintf(sql, "insert into offlinemessage(userid,message) values(%d,'%s')",
            userid, msg.c_str());
    // sprintf(sql, "insert into offlinemessage values(%d, '%s')", userid, msg.c_str());
    MySQL mysql;
    if (mysql.connect())
    {
        // 执行插入操作
        if (!mysql.update(sql))
        {
            LOG_ERROR << "Insert failed: " << mysql_error(mysql.getConnection());
        }
    }
}

// 删除用户的离线消息
void OfflineMessageModel::remove(int userid)
{
    char sql[1024] = {0};
    sprintf(sql, "delete from offlinemessage where userid = %d", userid);
    MySQL mysql;
    if (mysql.connect())
    {
        mysql.update(sql);
    }
}

// 查询用户的离线消息
std::vector<string> OfflineMessageModel::query(int userid)
{
    std::vector<string> _offlineMsg;
    char sql[1024] = {0};
    sprintf(sql, "select message from offlinemessage where userid = %d", userid);
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            // 每次调用mysql_fetch_row,它都会返回下一行的指针,直到没有更多行时,返回nullptr
            while ((row = mysql_fetch_row(res)) != nullptr) // 多行数据
                _offlineMsg.push_back(row[0]);
            mysql_free_result(res);
        }
    }
    return _offlineMsg;
}