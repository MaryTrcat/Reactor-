#pragma once

#include "business/AuthSession.h"

#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>

namespace business {

struct OnlineSession {
    std::string session_id;
    std::string username;
    std::string role;
    bool authenticated{false};
    std::string peer_address;
    std::int64_t last_active_at{0};
    // 该会话的登录时间；未登录会话为 0。
    std::int64_t authenticated_at{0};
};

class SessionRegistry {
public:
    void add(std::string session_id, std::shared_ptr<AuthSession> session);
    void remove(const std::string& session_id);
    bool forceLogout(const std::string& session_id);
    std::vector<OnlineSession> list() const;

private:
    struct Entry {
        std::shared_ptr<AuthSession> session;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> sessions_;
};

}  // namespace business
