#pragma once

#include "business/User.h"
#include "business/UserQuery.h"

#include <optional>
#include <string>
#include <vector>

namespace business {

class UserRepository {
public:
    virtual ~UserRepository() = default;

    virtual bool createUser(const std::string& username,
                            const std::string& password_hash,
                            const std::string& role,
                            std::int64_t& user_id,
                            std::string& error) = 0;
    virtual std::optional<User> findByUsername(const std::string& username) = 0;
    virtual bool updatePassword(const std::string& username,
                                const std::string& password_hash,
                                std::string& error) = 0;
    virtual bool updateStatus(const std::string& username,
                              bool frozen,
                              std::string& error) = 0;
    virtual bool deleteUser(const std::string& username, std::string& error) = 0;
    virtual std::vector<User> listUsers() = 0;
    // 带过滤、排序、分页的列表查询；total 是过滤后的总数而不是本页条数。
    virtual UserPage queryUsers(const UserQuery& query) = 0;
};

}  // namespace business
