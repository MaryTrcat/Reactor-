#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>

namespace business {

// 登录失败限流：按 username + 客户端地址做滑动窗口计数，超阈值后锁定一段时间。
//
// 目的有两个：挡住离线爆破，也顺带把 PBKDF2 的计算成本变成攻击者的成本。
// 数据只存在进程内存里，多实例部署时应换成 Redis（接口已按这个方向设计）。
class LoginAttemptTracker {
public:
    struct Config {
        int max_failures{5};
        int window_seconds{300};
        int lock_seconds{300};
        std::size_t max_entries{10000};
    };

    struct Decision {
        bool allowed{true};
        // 允许时表示还能试几次；拒绝时表示还要等多久。
        int remaining_attempts{0};
        int retry_after_seconds{0};
        std::string reason;
    };

    LoginAttemptTracker();
    explicit LoginAttemptTracker(Config config);

    // 查询当前是否允许尝试（不改变计数）。
    Decision check(const std::string& key);

    // 记录一次失败；返回记录后的决策。
    Decision onFailure(const std::string& key);

    // 登录成功：清空该 key 的失败记录。
    void onSuccess(const std::string& key);

    std::size_t size() const;
    void clear();

    static std::string makeKey(const std::string& username, const std::string& client_address);

private:
    struct Entry {
        std::deque<std::int64_t> failures;
        std::int64_t locked_until{0};
        std::int64_t last_touch{0};
    };

    static std::int64_t nowSeconds();
    void evictIfNeeded();
    int countRecentFailures(Entry& entry, std::int64_t now);

    Config config_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace business
