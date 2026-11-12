#pragma once

#include "business/UserQuery.h"
#include "business/UserRepository.h"

#include <memory>
#include <string>
#include <vector>

namespace business {

struct RegisterResult {
    bool success{};
    std::int64_t user_id{};
    std::string message;
};

struct LoginResult {
    bool success{};
    std::int64_t user_id{};
    std::string role;
    std::string message;
    // 老哈希校验通过后会自动升级存储格式，这里把结果带出来便于日志观察。
    bool upgraded_hash{false};
};

struct QueryResult {
    bool found{};
    std::int64_t user_id{};
    std::string username;
    std::string role;
    bool frozen{};
    std::string message;
};

struct PermissionResult {
    bool allowed{};
    std::string message;
};

struct OperationResult {
    bool success{};
    std::string message;
};

struct ResetPasswordResult {
    bool success{};
    std::string generated_password;
    std::string message;
};

struct ListResult {
    bool success{};
    std::vector<User> users;
    int total{0};
    int page{1};
    int page_size{UserQuery::kDefaultPageSize};
    std::string message;
};

class UserService {
public:
    explicit UserService(std::shared_ptr<UserRepository> repository);

    RegisterResult registerUser(const std::string& username, const std::string& password, const std::string& role);
    LoginResult login(const std::string& username, const std::string& password);
    QueryResult queryUser(const std::string& username);
    QueryResult queryUserAs(const std::string& actor, const std::string& username);
    PermissionResult checkPermission(const std::string& username, const std::string& required_role);
    PermissionResult checkAdminPermission(const std::string& username) const;
    OperationResult changePassword(const std::string& username, const std::string& new_password);
    // 修改自己的密码：必须校验旧密码，防止会话被劫持后直接改密。
    OperationResult changeOwnPassword(const std::string& actor,
                                      const std::string& old_password,
                                      const std::string& new_password);
    ResetPasswordResult adminResetPassword(const std::string& actor, const std::string& target);
    OperationResult adminSetFrozen(const std::string& actor, const std::string& target, bool frozen);
    OperationResult deleteUser(const std::string& username);
    OperationResult adminDeleteUser(const std::string& actor, const std::string& target);
    std::vector<User> listUsers();
    ListResult listUsersAs(const std::string& actor);
    ListResult queryUsersAs(const std::string& actor, const UserQuery& query);
    // 自主注册开关：开启后未登录用户可自助创建普通账号（角色强制为 user，
    // 不能自注册成 admin）；关闭时保持原行为——仅库里无用户时允许引导注册。
    void setAllowSelfRegister(bool enabled) { allow_self_register_ = enabled; }
    RegisterResult registerUserAs(const std::string& actor,
                                  const std::string& username,
                                  const std::string& password,
                                  const std::string& role);

    static bool isValidUsername(const std::string& username);

private:
    static int roleRank(const std::string& role);
    bool isAdmin(const std::string& username) const;
    OperationResult requireAdmin(const std::string& actor) const;
    static OperationResult validatePassword(const std::string& password);

    std::shared_ptr<UserRepository> repository_;
    bool allow_self_register_{false};
};

}  // namespace business
