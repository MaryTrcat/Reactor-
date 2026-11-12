#include "business/FileUserRepository.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace business {

FileUserRepository::FileUserRepository(std::string file_path) : file_path_(std::move(file_path)) {
    if (file_path_.empty()) {
        throw std::invalid_argument("file path must not be empty");
    }
    load();
}

bool FileUserRepository::createUser(const std::string& username,
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
    if (!save(error)) {
        users_.erase(username);
        --next_id_;
        return false;
    }

    error.clear();
    return true;
}

bool FileUserRepository::updateStatus(const std::string& username, bool frozen, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        error = "user not found";
        return false;
    }
    const auto previous = it->second.frozen;
    it->second.frozen = frozen;
    if (!save(error)) {
        it->second.frozen = previous;
        return false;
    }
    error.clear();
    return true;
}

std::optional<User> FileUserRepository::findByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = users_.find(username);
    if (it == users_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool FileUserRepository::updatePassword(const std::string& username,
                                        const std::string& password_hash,
                                        std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = users_.find(username);
    if (it == users_.end()) {
        error = "user not found";
        return false;
    }

    const auto previous_hash = it->second.password_hash;
    it->second.password_hash = password_hash;
    if (!save(error)) {
        it->second.password_hash = previous_hash;
        return false;
    }

    error.clear();
    return true;
}

bool FileUserRepository::deleteUser(const std::string& username, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = users_.find(username);
    if (it == users_.end()) {
        error = "user not found";
        return false;
    }

    const auto removed_user = it->second;
    users_.erase(it);
    if (!save(error)) {
        users_[username] = removed_user;
        return false;
    }

    error.clear();
    return true;
}

std::vector<User> FileUserRepository::listUsers() {
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

UserPage FileUserRepository::queryUsers(const UserQuery& query) {
    return paginateUsers(listUsers(), query);
}

void FileUserRepository::load() {
    std::lock_guard<std::mutex> lock(mutex_);
    users_.clear();
    next_id_ = 1;

    std::ifstream input(file_path_);
    if (!input.is_open()) {
        return;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream row(line);
        std::string id_text;
        User user;
        if (!std::getline(row, id_text, '\t') ||
            !std::getline(row, user.username, '\t') ||
            !std::getline(row, user.password_hash, '\t') ||
            !std::getline(row, user.role, '\t')) {
            continue;
        }

        try {
            user.id = std::stoll(id_text);
        } catch (const std::exception&) {
            continue;
        }

        std::string status;
        if (std::getline(row, status, '\t')) {
            user.frozen = status == "frozen" || status == "1";
        }

        users_[user.username] = user;
        next_id_ = std::max(next_id_, user.id + 1);
    }
}

bool FileUserRepository::save(std::string& error) const {
    const std::filesystem::path path(file_path_);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
    }

    std::vector<User> users;
    users.reserve(users_.size());
    for (const auto& entry : users_) {
        users.push_back(entry.second);
    }
    std::sort(users.begin(), users.end(), [](const User& left, const User& right) {
        return left.id < right.id;
    });

    std::ofstream output(file_path_, std::ios::trunc);
    if (!output.is_open()) {
        error = "failed to open user data file";
        return false;
    }

    for (const auto& user : users) {
        output << user.id << '\t'
               << user.username << '\t'
               << user.password_hash << '\t'
               << user.role << '\t'
               << (user.frozen ? "frozen" : "active") << '\n';
    }

    if (!output.good()) {
        error = "failed to write user data file";
        return false;
    }

    error.clear();
    return true;
}

}  // namespace business
