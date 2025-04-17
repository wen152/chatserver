#include <muduo/base/Logging.h>
#include "groupmodel.h"
#include "db.h"

// 创建群组
bool GroupModel::createGroup(Group &group)
{
    char sql[1024] = {0};
    sprintf(sql, "insert into allgroup(groupname,groupdesc) values('%s', '%s')", group.getName().c_str(), group.getDesc().c_str());
    MySQL mysql;
    if (mysql.connect())
    {
        if (mysql.update(sql))
        {
            // 获取插入成功的群组数据生成的主键id
            group.setId(mysql_insert_id(mysql.getConnection()));
            return true;
        }
    }
    return false;
}

// 加入群组
bool GroupModel::addGroup(int userid, int groupid, string role)
{
    char sql[1024] = {0};
    // 检查是否有这个群
    sprintf(sql, "select id from allgroup where id = %d", groupid);
    MySQL mysql;
    if (mysql.connect())
    {
        // 1. 检查群组是否存在
        sprintf(sql, "SELECT id FROM allgroup WHERE id = %d", groupid);
        MYSQL_RES *res = mysql.query(sql);
        if (res == nullptr)
        {
            LOG_ERROR << "Query failed: " << mysql_error(mysql.getConnection());
            return false; // 查询失败
        }
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row == nullptr)
        {
            mysql_free_result(res);
            LOG_ERROR << "The group does not exist: groupid = " << groupid;
            return false; // 群组不存在
        }
        mysql_free_result(res);

        // 2. 检查用户是否已经在群组中
        sprintf(sql, "SELECT userid FROM groupuser WHERE groupid = %d AND userid=%d", groupid, userid);
        res = mysql.query(sql);
        if (res == nullptr)
        {
            LOG_ERROR << "Query failed: " << mysql_error(mysql.getConnection());
            return false; // 查询失败
        }
        row = mysql_fetch_row(res);
        if (row != nullptr)
        {
            mysql_free_result(res);
            LOG_INFO << "User already exists in the group: (" << userid << ", " << groupid << ")";
            return false; // 用户已经在群组中
        }
        mysql_free_result(res);

        // 3. 插入群组用户关系
        sprintf(sql, "INSERT INTO groupuser VALUES(%d, %d, '%s')", groupid, userid, role.c_str());
        if (!mysql.update(sql))
        {
            LOG_ERROR << "Insert failed: " << mysql_error(mysql.getConnection());
            return false; // 插入失败
        }
        return true;
    }
}

// 查询是否在这个群
bool GroupModel::isGroupUser(int userid, int groupid)
{
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid = %d", groupid, userid);
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row != nullptr)
            {
                mysql_free_result(res);
                return true; // 在这个群组里
            }
            mysql_free_result(res);
        }
    }
    return false; // 不在这个群组里
}

// 查询用户所在群组信息
vector<Group> GroupModel::queryGroups(int userid)
{
    vector<Group> _groupvec;
    /*
    1. 先根据userid在groupuser表中查询出该用户所属的群组信息
    2. 在根据群组信息，查询属于该群组的所有用户的userid，并且和user表进行多表联合查询，查出用户的详细信息
    */
    char sql[1024] = {0};
    sprintf(sql, "select a.id,a.groupname,a.groupdesc from allgroup a inner join \
        groupuser b on a.id = b.groupid where b.userid=%d",
            userid);

    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                // 查出userid所有的群组信息
                Group _group;
                _group.setId(atoi(row[0]));
                _group.setName(row[1]);
                _group.setDesc(row[2]);
                _groupvec.push_back(_group);
            }
            mysql_free_result(res);
        }
    }
    // 查询群组的用户信息
    for (auto &_group : _groupvec)
    {
        // 在groupuser 找group_id -> user_id -> 多组user数据
        sprintf(sql, "select a.id,a.name,a.state,b.grouprole from user a \
            inner join groupuser b on b.userid = a.id where b.groupid=%d",
                _group.getId());
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
            {
                GroupUser user;
                user.setId(atoi(row[0]));
                user.setName(row[1]);
                user.setState(row[2]);
                user.setRole(row[3]);
                _group.getUsers().push_back(user);
            }
            mysql_free_result(res);
        }
    }
    return _groupvec;
}

// 根据指定的groupid查询群组用户id列表,除userid自己,主要用户群聊业务给群组其它成员群发消息
vector<int> GroupModel::queryGroupUsers(int userid, int groupid)
{
    vector<int> _groupuser;
    char sql[1024] = {0};
    sprintf(sql, "select userid from groupuser where groupid = %d and userid != %d", groupid, userid);
    MySQL mysql;
    if (mysql.connect())
    {
        MYSQL_RES *res = mysql.query(sql);
        if (res != nullptr)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res)) != nullptr)
                _groupuser.push_back(atoi(row[0]));
            mysql_free_result(res);
        }
    }
    return _groupuser;
}