#include "business/LoginAttemptTracker.h"

#include <algorithm>
#include <ctime>

namespace business {

LoginAttemptTracker::LoginAttemptTracker() : LoginAttemptTracker(Config{}) {
}

LoginAttemptTracker::LoginAttemptTracker(Config config) : config_(config) {
    if (config_.max_failures <= 0) {
        config_.max_failures = 5;
    }
    if (config_.window_seconds <= 0) {
        config_.window_seconds = 300;
    }
    if (config_.lock_seconds <= 0) {
        config_.lock_seconds = 300;
    }
    if (config_.max_entries == 0) {
        config_.max_entries = 10000;
    }
}

std::string LoginAttemptTracker::makeKey(const std::string& username, const std::string& client_address) {
    return username + "@" + (client_address.empty() ? "unknown" : client_address);
}

std::int64_t LoginAttemptTracker::nowSeconds() {
    return static_cast<std::int64_t>(::time(nullptr));
}

int LoginAttemptTracker::countRecentFailures(Entry& entry, std::int64_t now) {
    const auto window_start = now - config_.window_seconds;
    while (!entry.failures.empty() && entry.failures.front() < window_start) {
        entry.failures.pop_front();
    }
    return static_cast<int>(entry.failures.size());
}

LoginAttemptTracker::Decision LoginAttemptTracker::check(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = nowSeconds();

    const auto it = entries_.find(key);
    if (it == entries_.end()) {
        return {true, config_.max_failures, 0, ""};
    }

    auto& entry = it->second;
    if (entry.locked_until > now) {
        return {false, 0, static_cast<int>(entry.locked_until - now), "too many failed attempts"};
    }

    const int recent = countRecentFailures(entry, now);
    if (recent >= config_.max_failures) {
        // 窗口内失败次数已满：进入锁定期。
        entry.locked_until = now + config_.lock_seconds;
        entry.last_touch = now;
        return {false, 0, config_.lock_seconds, "too many failed attempts"};
    }
    return {true, config_.max_failures - recent, 0, ""};
}

LoginAttemptTracker::Decision LoginAttemptTracker::onFailure(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = nowSeconds();

    evictIfNeeded();

    auto& entry = entries_[key];
    entry.last_touch = now;
    entry.failures.push_back(now);
    const int recent = countRecentFailures(entry, now);

    if (recent >= config_.max_failures) {
        entry.locked_until = now + config_.lock_seconds;
        return {false, 0, config_.lock_seconds, "too many failed attempts"};
    }
    return {true, config_.max_failures - recent, 0, ""};
}

void LoginAttemptTracker::onSuccess(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.erase(key);
}

std::size_t LoginAttemptTracker::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return entries_.size();
}

void LoginAttemptTracker::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entries_.clear();
}

void LoginAttemptTracker::evictIfNeeded() {
    if (entries_.size() < config_.max_entries) {
        return;
    }
    // 内存上限保护：先清掉窗口已过期的记录，仍超限就丢弃最久未访问的条目。
    const auto now = nowSeconds();
    for (auto it = entries_.begin(); it != entries_.end();) {
        const bool expired = it->second.locked_until <= now &&
                             countRecentFailures(it->second, now) == 0;
        if (expired) {
            it = entries_.erase(it);
            continue;
        }
        ++it;
    }

    while (entries_.size() >= config_.max_entries) {
        auto oldest = entries_.begin();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (it->second.last_touch < oldest->second.last_touch) {
                oldest = it;
            }
        }
        entries_.erase(oldest);
    }
}

}  // namespace business
