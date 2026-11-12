#include "business/SessionRegistry.h"

#include <algorithm>

namespace business {

void SessionRegistry::add(std::string session_id, std::shared_ptr<AuthSession> session) {
    if (session_id.empty() || !session) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_[std::move(session_id)] = {std::move(session)};
}

void SessionRegistry::remove(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    sessions_.erase(session_id);
}

bool SessionRegistry::forceLogout(const std::string& session_id) {
    std::shared_ptr<AuthSession> session;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = sessions_.find(session_id);
        if (it == sessions_.end()) {
            return false;
        }
        session = it->second.session;
    }
    session->clear();
    return true;
}

std::vector<OnlineSession> SessionRegistry::list() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<OnlineSession> result;
    result.reserve(sessions_.size());
    for (const auto& [session_id, entry] : sessions_) {
        const auto snapshot = entry.session->snapshot();
        result.push_back({session_id,
                          snapshot.username,
                          snapshot.role,
                          snapshot.authenticated,
                          snapshot.peer_address,
                          snapshot.last_active_at,
                          snapshot.authenticated_at});
    }
    std::sort(result.begin(), result.end(), [](const OnlineSession& left, const OnlineSession& right) {
        return left.session_id < right.session_id;
    });
    return result;
}

}  // namespace business
