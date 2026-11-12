#pragma once

#include "business/UserRepository.h"
#include "db/MySqlConnectionPool.h"

#include <memory>

namespace db {

class MySqlUserRepository : public business::UserRepository {
public:
    explicit MySqlUserRepository(std::shared_ptr<MySqlConnectionPool> pool);

    bool createUser(const std::string& username,
                    const std::string& password_hash,
                    const std::string& role,
                    std::int64_t& user_id,
                    std::string& error) override;
    std::optional<business::User> findByUsername(const std::string& username) override;
    bool updatePassword(const std::string& username,
                        const std::string& password_hash,
                        std::string& error) override;
    bool updateStatus(const std::string& username, bool frozen, std::string& error) override;
    bool deleteUser(const std::string& username, std::string& error) override;
    std::vector<business::User> listUsers() override;
    business::UserPage queryUsers(const business::UserQuery& query) override;

private:
    std::shared_ptr<MySqlConnectionPool> pool_;
};

}  // namespace db
