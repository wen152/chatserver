#ifndef DB_H
#define DB_H
#include <mysql/mysql.h>
#include <string>
using std::string;
// 数据库操作类
class MySQL
{
public:
    // 初始化数据库连接
    MySQL()
    {
        _conn = mysql_init(nullptr);
    }
    // 释放数据库连接资源
    ~MySQL()
    {
        if (_conn != nullptr)
            mysql_close(_conn);
    }
    // 连接数据库
    bool connect();
    // 更新操作
    bool update(string);
    // 查询操作
    MYSQL_RES *query(string);
    MYSQL *getConnection();

private:
    MYSQL *_conn;
};
#endif