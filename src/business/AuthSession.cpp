#include "business/AuthSession.h"

namespace business {

void AuthSession::authenticate(std::string username, std::string role, std::int64_t authenticated_at) {
    std::lock_guard<std::mutex> lock(mutex_);
    authenticated_ = true;
    username_ = std::move(username);
    role_ = std::move(role);
    authenticated_at_ = authenticated_at;
}

void AuthSession::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    authenticated_ = false;
    username_.clear();
    role_.clear();
    authenticated_at_ = 0;
}

AuthSession::Snapshot AuthSession::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return Snapshot{authenticated_, username_, role_, peer_address_, last_active_at_, authenticated_at_};
}

void AuthSession::setPeerAddress(std::string peer_address) {
    std::lock_guard<std::mutex> lock(mutex_);
    peer_address_ = std::move(peer_address);
}

void AuthSession::touch(std::int64_t now_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    last_active_at_ = now_seconds;
}

}  // namespace business
