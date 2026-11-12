#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace business {

// 绑定在 TCP 连接上的登录身份。所有读写都在连接线程/工作线程之间共享，
// 因此用互斥锁保护，并且在任何时刻只把 Snapshot 拷贝出去使用。
class AuthSession {
public:
    struct Snapshot {
        bool authenticated{false};
        std::string username;
        std::string role;
        std::string peer_address;
        std::int64_t last_active_at{0};
        std::int64_t authenticated_at{0};
    };

    void authenticate(std::string username, std::string role, std::int64_t authenticated_at = 0);
    void clear();
    Snapshot snapshot() const;

    // 连接层在建立连接/收到数据时调用，用于在线列表和空闲超时展示。
    void setPeerAddress(std::string peer_address);
    void touch(std::int64_t now_seconds);

private:
    mutable std::mutex mutex_;
    bool authenticated_{false};
    std::string username_;
    std::string role_;
    std::string peer_address_;
    std::int64_t last_active_at_{0};
    std::int64_t authenticated_at_{0};
};

}  // namespace business
