#ifndef USER_H
#define USER_H
#include <string>
using std::string;
// 映射类 方便提交给业务处理
// 匹配User表的ORM类
class User
{
public:
    // 默认状态为offline
    User(int id = -1, string name = "", string password = "", string state = "offline")
        : _id(id), _name(name), _password(password), _state(state) {}

    void setId(int id) { this->_id = id; }
    void setName(string name) { this->_name = name; }
    void setPassword(string password) { this->_password = password; }
    void setState(string state) { this->_state = state; }

    int getId() const { return this->_id; }
    string getName() const { return this->_name; }
    string getPassword() const { return this->_password; }
    string getState() const { return this->_state; }

private:
    int _id; // 主键
    string _name;
    string _password;
    string _state;
};

#endif