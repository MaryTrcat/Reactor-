#include "db/MySqlUserRepository.h"

#include <sstream>
#include <stdexcept>

namespace db {

MySqlUserRepository::MySqlUserRepository(std::shared_ptr<MySqlConnectionPool> pool) : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("pool must not be null");
    }
}

bool MySqlUserRepository::createUser(const std::string& username,
                                     const std::string& password_hash,
                                     const std::string& role,
                                     std::int64_t& user_id,
                                     std::string& error) {
    auto connection = pool_->acquire();
    std::ostringstream sql;
    sql << "INSERT INTO users(username, password_hash, role, frozen) VALUES('"
        << connection->escape(username) << "', '"
        << connection->escape(password_hash) << "', '"
        << connection->escape(role.empty() ? "user" : role) << "', 0)";

    if (!connection->execute(sql.str(), error)) {
        return false;
    }

    user_id = static_cast<std::int64_t>(connection->lastInsertId());
    return true;
}

std::optional<business::User> MySqlUserRepository::findByUsername(const std::string& username) {
    auto connection = pool_->acquire();
    std::string error;
    std::ostringstream sql;
    sql << "SELECT id, username, password_hash, role, frozen FROM users WHERE username='"
        << connection->escape(username) << "' LIMIT 1";

    const auto rows = connection->query(sql.str(), error);
    if (rows.empty() || rows.front().size() < 5) {
        return std::nullopt;
    }

    business::User user;
    user.id = std::stoll(rows.front()[0]);
    user.username = rows.front()[1];
    user.password_hash = rows.front()[2];
    user.role = rows.front()[3];
    user.frozen = rows.front()[4] == "1";
    return user;
}

bool MySqlUserRepository::updatePassword(const std::string& username,
                                         const std::string& password_hash,
                                         std::string& error) {
    auto connection = pool_->acquire();
    std::ostringstream sql;
    sql << "UPDATE users SET password_hash='" << connection->escape(password_hash)
        << "' WHERE username='" << connection->escape(username) << "'";

    if (!connection->execute(sql.str(), error)) {
        return false;
    }
    if (connection->affectedRows() == 0) {
        error = "user not found";
        return false;
    }
    return true;
}

bool MySqlUserRepository::updateStatus(const std::string& username, bool frozen, std::string& error) {
    auto connection = pool_->acquire();
    std::ostringstream sql;
    sql << "UPDATE users SET frozen=" << (frozen ? 1 : 0)
        << " WHERE username='" << connection->escape(username) << "'";
    if (!connection->execute(sql.str(), error)) {
        return false;
    }
    if (connection->affectedRows() == 0) {
        error = "user not found";
        return false;
    }
    return true;
}

bool MySqlUserRepository::deleteUser(const std::string& username, std::string& error) {
    auto connection = pool_->acquire();
    std::ostringstream sql;
    sql << "DELETE FROM users WHERE username='" << connection->escape(username) << "'";

    if (!connection->execute(sql.str(), error)) {
        return false;
    }
    if (connection->affectedRows() == 0) {
        error = "user not found";
        return false;
    }
    return true;
}

std::vector<business::User> MySqlUserRepository::listUsers() {
    auto connection = pool_->acquire();
    std::string error;
    const auto rows = connection->query("SELECT id, username, password_hash, role, frozen FROM users ORDER BY id", error);

    std::vector<business::User> users;
    users.reserve(rows.size());
    for (const auto& row : rows) {
        if (row.size() < 5) {
            continue;
        }

        business::User user;
        user.id = std::stoll(row[0]);
        user.username = row[1];
        user.password_hash = row[2];
        user.role = row[3];
        user.frozen = row[4] == "1";
        users.push_back(user);
    }
    return users;
}

business::UserPage MySqlUserRepository::queryUsers(const business::UserQuery& raw_query) {
    business::UserQuery query = raw_query;
    query.normalize();

    auto connection = pool_->acquire();
    std::string error;

    // 过滤条件下推到 SQL，避免把整张表捞到内存里再切页。
    std::string where = " WHERE 1=1";
    if (!query.keyword.empty()) {
        // 前缀匹配能吃到 username 上的唯一索引，全模糊匹配只能全表扫描。
        where += " AND username LIKE '" + connection->escape(query.keyword) + "%'";
    }
    if (!query.status.empty()) {
        where += std::string(" AND frozen=") + (query.status == "frozen" ? "1" : "0");
    }
    if (!query.role.empty()) {
        where += " AND role='" + connection->escape(query.role) + "'";
    }

    business::UserPage page;
    page.page = query.page;
    page.page_size = query.page_size;

    const auto count_rows = connection->query("SELECT COUNT(*) FROM users" + where, error);
    if (!count_rows.empty() && !count_rows.front().empty()) {
        try {
            page.total = std::stoi(count_rows.front().front());
        } catch (const std::exception&) {
            page.total = 0;
        }
    }

    std::string order_column = "id";
    if (query.order_by == "username") {
        order_column = "username";
    } else if (query.order_by == "status") {
        order_column = "frozen";
    }
    const std::string direction = query.descending ? " DESC" : " ASC";
    const long long offset = static_cast<long long>(query.page - 1) * query.page_size;

    std::ostringstream sql;
    sql << "SELECT id, username, password_hash, role, frozen FROM users" << where
        << " ORDER BY " << order_column << direction << ", id" << direction
        << " LIMIT " << query.page_size << " OFFSET " << offset;

    const auto rows = connection->query(sql.str(), error);
    page.users.reserve(rows.size());
    for (const auto& row : rows) {
        if (row.size() < 5) {
            continue;
        }
        business::User user;
        user.id = std::stoll(row[0]);
        user.username = row[1];
        user.password_hash = row[2];
        user.role = row[3];
        user.frozen = row[4] == "1";
        page.users.push_back(std::move(user));
    }
    return page;
}

}  // namespace db
