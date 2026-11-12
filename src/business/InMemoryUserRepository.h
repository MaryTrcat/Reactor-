#pragma once

#include "business/UserRepository.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace business {

class InMemoryUserRepository : public UserRepository {
public:
    bool createUser(const std::string& username,
                    const std::string& password_hash,
                    const std::string& role,
                    std::int64_t& user_id,
                    std::string& error) override;
    std::optional<User> findByUsername(const std::string& username) override;
    bool updatePassword(const std::string& username,
                        const std::string& password_hash,
                        std::string& error) override;
    bool updateStatus(const std::string& username, bool frozen, std::string& error) override;
    bool deleteUser(const std::string& username, std::string& error) override;
    std::vector<User> listUsers() override;
    UserPage queryUsers(const UserQuery& query) override;

private:
    std::mutex mutex_;
    std::int64_t next_id_{1};
    std::unordered_map<std::string, User> users_;
};

}  // namespace business
