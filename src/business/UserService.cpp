#include "business/UserService.h"

#include "business/PasswordHasher.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace business {

namespace {

constexpr std::size_t kMinUsernameLength = 3;
constexpr std::size_t kMaxUsernameLength = 32;
constexpr std::size_t kMaxPasswordLength = 128;

std::string normalizeRole(const std::string& role) {
    return role == "admin" ? "admin" : "user";
}

}  // namespace

UserService::UserService(std::shared_ptr<UserRepository> repository) : repository_(std::move(repository)) {
    if (!repository_) {
        throw std::invalid_argument("repository must not be null");
    }
}

bool UserService::isValidUsername(const std::string& username) {
    if (username.size() < kMinUsernameLength || username.size() > kMaxUsernameLength) {
        return false;
    }
    for (const char ch : username) {
        const auto uc = static_cast<unsigned char>(ch);
        if (std::isalnum(uc) == 0 && ch != '_' && ch != '-') {
            return false;
        }
    }
    return true;
}

RegisterResult UserService::registerUser(const std::string& username, const std::string& password, const std::string& role) {
    if (!isValidUsername(username)) {
        return {false, 0, "username must be 3-32 characters of letters, digits, '_' or '-'"};
    }
    if (password.size() < 6) {
        return {false, 0, "password must contain at least 6 characters"};
    }
    if (password.size() > kMaxPasswordLength) {
        return {false, 0, "password is too long"};
    }

    const auto effective_role = normalizeRole(role);
    if (effective_role == "admin") {
        // 超级管理员唯一：库里已经有管理员时不允许再创建 admin。
        // 只有首次初始化（一个用户都没有）时才可能落到这里创建第一个管理员。
        UserQuery probe;
        probe.page = 1;
        probe.page_size = 1;
        probe.role = "admin";
        if (repository_->queryUsers(probe).total > 0) {
            return {false, 0, "only one super admin is allowed"};
        }
    }

    std::int64_t user_id = 0;
    std::string error;
    const auto saved = repository_->createUser(username, PasswordHasher::hash(password), effective_role, user_id, error);
    if (!saved) {
        return {false, 0, error.empty() ? "failed to create user" : error};
    }

    return {true, user_id, "registered"};
}

LoginResult UserService::login(const std::string& username, const std::string& password) {
    const auto user = repository_->findByUsername(username);
    if (!user.has_value()) {
        // 不区分"用户不存在"和"密码错误"，避免被用来枚举账号。
        return {false, 0, "", "invalid username or password", false};
    }

    bool needs_rehash = false;
    if (!PasswordHasher::verify(password, user->password_hash, needs_rehash)) {
        return {false, 0, "", "invalid username or password", false};
    }
    if (user->frozen) {
        return {false, 0, "", "account is frozen", false};
    }

    if (needs_rehash) {
        // 老哈希或低迭代次数哈希：登录成功后原地升级，数据无需停机迁移。
        std::string error;
        repository_->updatePassword(user->username, PasswordHasher::hash(password), error);
    }

    return {true, user->id, user->role, "login ok", needs_rehash};
}

QueryResult UserService::queryUser(const std::string& username) {
    const auto user = repository_->findByUsername(username);
    if (!user.has_value()) {
        return {false, 0, "", "", false, "user not found"};
    }
    return {true, user->id, user->username, user->role, user->frozen, "found"};
}

QueryResult UserService::queryUserAs(const std::string& actor, const std::string& username) {
    const auto actor_user = repository_->findByUsername(actor);
    if (!actor_user.has_value()) {
        return {false, 0, "", "", false, "authentication required"};
    }
    if (actor != username && actor_user->role != "admin") {
        return {false, 0, "", "", false, "permission denied"};
    }
    return queryUser(username);
}

PermissionResult UserService::checkPermission(const std::string& username, const std::string& required_role) {
    const auto user = repository_->findByUsername(username);
    if (!user.has_value()) {
        return {false, "user not found"};
    }
    if (roleRank(user->role) < roleRank(required_role)) {
        return {false, "permission denied"};
    }
    return {true, "permission granted"};
}

PermissionResult UserService::checkAdminPermission(const std::string& username) const {
    const auto result = requireAdmin(username);
    return {result.success, result.message};
}

OperationResult UserService::changePassword(const std::string& username, const std::string& new_password) {
    if (username.empty()) {
        return {false, "username is required"};
    }
    const auto validation = validatePassword(new_password);
    if (!validation.success) {
        return validation;
    }

    std::string error;
    if (!repository_->updatePassword(username, PasswordHasher::hash(new_password), error)) {
        return {false, error.empty() ? "failed to change password" : error};
    }
    return {true, "password changed"};
}

OperationResult UserService::changeOwnPassword(const std::string& actor,
                                               const std::string& old_password,
                                               const std::string& new_password) {
    if (actor.empty()) {
        return {false, "authentication required"};
    }
    const auto user = repository_->findByUsername(actor);
    if (!user.has_value()) {
        return {false, "user not found"};
    }
    bool needs_rehash = false;
    if (!PasswordHasher::verify(old_password, user->password_hash, needs_rehash)) {
        return {false, "old password is incorrect"};
    }
    if (old_password == new_password) {
        return {false, "new password must differ from the old one"};
    }
    return changePassword(actor, new_password);
}

ResetPasswordResult UserService::adminResetPassword(const std::string& actor, const std::string& target) {
    const auto permission = requireAdmin(actor);
    if (!permission.success) {
        return {false, "", permission.message};
    }
    if (target.empty()) {
        return {false, "", "target username is required"};
    }

    // 不再固定重置成 123456：随机口令只回传一次，管理员自行转达。
    const auto generated = PasswordHasher::generatePassword(12);
    std::string error;
    if (!repository_->updatePassword(target, PasswordHasher::hash(generated), error)) {
        return {false, "", error.empty() ? "failed to reset password" : error};
    }
    return {true, generated, "password reset, please deliver it to the user"};
}

OperationResult UserService::adminSetFrozen(const std::string& actor, const std::string& target, bool frozen) {
    const auto permission = requireAdmin(actor);
    if (!permission.success) {
        return permission;
    }
    if (target.empty()) {
        return {false, "target username is required"};
    }
    if (actor == target && frozen) {
        return {false, "cannot freeze the current admin account"};
    }
    std::string error;
    if (!repository_->updateStatus(target, frozen, error)) {
        return {false, error.empty() ? "failed to update account status" : error};
    }
    return {true, frozen ? "account frozen" : "account unfrozen"};
}

OperationResult UserService::deleteUser(const std::string& username) {
    if (username.empty()) {
        return {false, "username is required"};
    }

    std::string error;
    if (!repository_->deleteUser(username, error)) {
        return {false, error.empty() ? "failed to delete user" : error};
    }
    return {true, "user deleted"};
}

OperationResult UserService::adminDeleteUser(const std::string& actor, const std::string& target) {
    const auto permission = requireAdmin(actor);
    if (!permission.success) {
        return permission;
    }
    if (actor == target) {
        return {false, "cannot delete the current admin account"};
    }
    return deleteUser(target);
}

std::vector<User> UserService::listUsers() {
    auto users = repository_->listUsers();
    for (auto& user : users) {
        user.password_hash.clear();
    }
    return users;
}

ListResult UserService::listUsersAs(const std::string& actor) {
    UserQuery query;
    query.page_size = UserQuery::kMaxPageSize;
    return queryUsersAs(actor, query);
}

ListResult UserService::queryUsersAs(const std::string& actor, const UserQuery& query) {
    const auto permission = requireAdmin(actor);
    if (!permission.success) {
        return {false, {}, 0, 1, UserQuery::kDefaultPageSize, permission.message};
    }

    const auto page = repository_->queryUsers(query);
    ListResult result;
    result.success = true;
    result.users = page.users;
    result.total = page.total;
    result.page = page.page;
    result.page_size = page.page_size;
    result.message = "users listed";
    for (auto& user : result.users) {
        user.password_hash.clear();
    }
    return result;
}

RegisterResult UserService::registerUserAs(const std::string& actor,
                                           const std::string& username,
                                           const std::string& password,
                                           const std::string& role) {
    if (actor.empty()) {
        // 首次初始化：库里没有任何用户时允许免鉴权创建第一个管理员。
        UserQuery probe;
        probe.page = 1;
        probe.page_size = 1;
        if (repository_->queryUsers(probe).total > 0) {
            if (!allow_self_register_) {
                return {false, 0, "authentication required"};
            }
            // 自主注册：角色强制为普通用户，请求里带的 role 不可信。
            return registerUser(username, password, "user");
        }
        return registerUser(username, password, normalizeRole(role));
    }
    const auto permission = requireAdmin(actor);
    if (!permission.success) {
        return {false, 0, permission.message};
    }
    return registerUser(username, password, role);
}

int UserService::roleRank(const std::string& role) {
    if (role == "admin") {
        return 2;
    }
    if (role == "user") {
        return 1;
    }
    return 0;
}

bool UserService::isAdmin(const std::string& username) const {
    const auto user = repository_->findByUsername(username);
    return user.has_value() && !user->frozen && user->role == "admin";
}

OperationResult UserService::requireAdmin(const std::string& actor) const {
    if (actor.empty()) {
        return {false, "authentication required"};
    }
    if (!isAdmin(actor)) {
        return {false, "admin permission required"};
    }
    return {true, "permission granted"};
}

OperationResult UserService::validatePassword(const std::string& password) {
    if (password.size() < 6) {
        return {false, "password must contain at least 6 characters"};
    }
    if (password.size() > kMaxPasswordLength) {
        return {false, "password is too long"};
    }
    return {true, "password valid"};
}

}  // namespace business
