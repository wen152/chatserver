#include <muduo/base/Logging.h>
#include "db.h"

// 数据库配置信息
static string server = "127.0.0.1"; // IP
static string user = "root";        // 用户
static string password = "123456";  // 密码
static string dbname = "chat";      // 数据库名称

// 连接数据库
bool MySQL::connect()
{
    // 用户名 密码 端口号
    MYSQL *p = mysql_real_connect(_conn, server.c_str(), user.c_str(),
                                  password.c_str(), dbname.c_str(), 3306, nullptr, 0);
    if (p != nullptr)
    {
        // 代码默认的编码字符是ASCII，如果不设置，从MySQL上拉下来的中文显示？
        mysql_query(_conn, "set names gbk");
        LOG_INFO << "connect mysql success!";
    }
    else
    {
        LOG_INFO << "connect mysql fail!";
    }
    return p;
}
// 更新操作
bool MySQL::update(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "更新失败!";
        return false;
    }
    return true;
}
// 查询操作
MYSQL_RES *MySQL::query(string sql)
{
    if (mysql_query(_conn, sql.c_str()))
    {
        LOG_INFO << __FILE__ << ":" << __LINE__ << ":"
                 << sql << "查询失败!";
        return nullptr;
    }
    return mysql_use_result(_conn); // mysql_num_rows 只能在使用 mysql_store_result 时返回结果集的行数
}

MYSQL *MySQL::getConnection()
{
    return _conn;
}