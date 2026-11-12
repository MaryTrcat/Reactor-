#include "business/InMemoryUserRepository.h"

#include <algorithm>

namespace business {

bool InMemoryUserRepository::createUser(const std::string& username,
                                        const std::string& password_hash,
                                        const std::string& role,
                                        std::int64_t& user_id,
                                        std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (users_.find(username) != users_.end()) {
        error = "username already exists";
        return false;
    }

    user_id = next_id_++;
    users_[username] = User{user_id, username, password_hash, role.empty() ? "user" : role};
    error.clear();
    return true;
}

bool InMemoryUserRepository::updateStatus(const std::string& username, bool frozen, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        error = "user not found";
        return false;
    }
    it->second.frozen = frozen;
    error.clear();
    return true;
}

std::optional<User> InMemoryUserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = users_.find(username);
    if (it == users_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool InMemoryUserRepository::updatePassword(const std::string& username,
                                            const std::string& password_hash,
                                            std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        error = "user not found";
        return false;
    }

    it->second.password_hash = password_hash;
    error.clear();
    return true;
}

bool InMemoryUserRepository::deleteUser(const std::string& username, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto removed = users_.erase(username);
    if (removed == 0) {
        error = "user not found";
        return false;
    }

    error.clear();
    return true;
}

std::vector<User> InMemoryUserRepository::listUsers() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<User> users;
    users.reserve(users_.size());
    for (const auto& entry : users_) {
        users.push_back(entry.second);
    }

    std::sort(users.begin(), users.end(), [](const User& left, const User& right) {
        return left.id < right.id;
    });
    return users;
}

UserPage InMemoryUserRepository::queryUsers(const UserQuery& query) {
    return paginateUsers(listUsers(), query);
}

}  // namespace business
