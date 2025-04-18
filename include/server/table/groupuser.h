#ifndef GROUPUSER_H
#define GROUPUSER_H
// 中间表 GroupUser
#include "user.h"

// 群组用户，多了一个role角色信息，从User类直接继承，复用User的其它信息
class GroupUser : public User
{
public:
    void setRole(string role) { this->role = role; }
    string getRole() const { return this->role; }

private:
    string role;
};

#endif