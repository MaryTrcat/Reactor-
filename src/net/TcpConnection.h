#pragma once

#include "net/Buffer.h"
#include "protocol/Codec.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace business { class AuthSession; }

namespace net {

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    // 输出缓冲高水位：慢客户端（只读不写或网络卡住）会把待发数据堆在内存里，
    // 超过该阈值就直接判定为不可恢复的背压，交给事件循环关闭连接。
    static constexpr std::size_t kMaxOutputBufferSize = 4 * 1024 * 1024;

    using MessageCallback = std::function<void(std::shared_ptr<TcpConnection>, const protocol::Frame&)>;
    using CloseCallback = std::function<void(int)>;
    using WakeupCallback = std::function<void()>;

    explicit TcpConnection(int fd);
    ~TcpConnection();

    TcpConnection(const TcpConnection&) = delete;
    TcpConnection& operator=(const TcpConnection&) = delete;

    int fd() const;
    void setMessageCallback(MessageCallback callback);
    void setCloseCallback(CloseCallback callback);
    void setWakeupCallback(WakeupCallback callback);
    bool handleRead();
    bool handleWrite();
    void queueFrame(const protocol::Frame& frame);
    bool hasPendingOutput() const;
    void send(const protocol::Frame& frame);
    std::shared_ptr<business::AuthSession> session() const;

    void setPeerAddress(std::string peer_address);
    std::string peerAddress() const;
    void touch();
    std::int64_t lastActiveAt() const;
    bool idleFor(std::int64_t now_seconds, int idle_timeout_seconds) const;

    // 由任意线程置位，事件循环在下一轮把它转成真正的 close。
    // 这样写线程不需要碰 epoll 的 fd 表，避免跨线程操作连接容器。
    void requestClose();
    void requestClose(const std::string& reason);
    bool closeRequested() const;
    const std::string& closeReason() const;

private:
    bool fail(const std::string& reason);

    int fd_{-1};
    protocol::Codec codec_;
    protocol::Codec encoder_;
    Buffer output_;
    mutable std::mutex output_mutex_;
    MessageCallback message_callback_;
    CloseCallback close_callback_;
    WakeupCallback wakeup_callback_;
    std::shared_ptr<business::AuthSession> session_;

    mutable std::mutex meta_mutex_;
    std::string peer_address_;
    std::atomic<std::int64_t> last_active_at_{0};

    std::atomic<bool> close_requested_{false};
    std::string close_reason_;
};

}  // namespace net
