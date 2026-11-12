#include "net/TcpConnection.h"

#include "base/Logger.h"
#include "business/AuthSession.h"
#include "net/SocketUtil.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

namespace {

std::string describePeer(int fd) {
    sockaddr_in address{};
    socklen_t length = sizeof(address);
    if (getpeername(fd, reinterpret_cast<sockaddr*>(&address), &length) != 0) {
        return "unknown";
    }
    char host[INET_ADDRSTRLEN]{};
    if (inet_ntop(AF_INET, &address.sin_addr, host, sizeof(host)) == nullptr) {
        return "unknown";
    }
    return std::string(host) + ":" + std::to_string(ntohs(address.sin_port));
}

}  // namespace

TcpConnection::TcpConnection(int fd) : fd_(fd), session_(std::make_shared<business::AuthSession>()) {
    peer_address_ = describePeer(fd);
    last_active_at_.store(static_cast<std::int64_t>(::time(nullptr)));
    session_->setPeerAddress(peer_address_);
}

TcpConnection::~TcpConnection() {
    closeFd(fd_);
}

int TcpConnection::fd() const {
    return fd_;
}

void TcpConnection::setMessageCallback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void TcpConnection::setCloseCallback(CloseCallback callback) {
    close_callback_ = std::move(callback);
}

void TcpConnection::setWakeupCallback(WakeupCallback callback) {
    wakeup_callback_ = std::move(callback);
}

bool TcpConnection::handleRead() {
    std::uint8_t buffer[4096];
    while (true) {
        const auto n = recv(fd_, buffer, sizeof(buffer), 0);
        if (n > 0) {
            touch();
            codec_.append(buffer, static_cast<std::size_t>(n));
            try {
                while (auto frame = codec_.tryDecode()) {
                    if (message_callback_) {
                        message_callback_(shared_from_this(), *frame);
                    }
                }
            } catch (const std::exception& ex) {
                // 业务回调抛异常绝不能穿回 epoll 循环，否则整个进程 terminate。
                base::logError("message callback failed fd=" + std::to_string(fd_) + ": " + ex.what());
                return fail("message callback exception");
            }
            if (codec_.failed()) {
                return fail(codec_.error());
            }
            continue;
        }
        if (n == 0) {
            return fail("peer closed");
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        return fail(std::string("recv failed: ") + std::strerror(errno));
    }
}

bool TcpConnection::handleWrite() {
    // 先在锁内把待发数据整体取走，再在锁外 send：
    // 慢速对端触发 EAGAIN 时不会把业务线程堵在 output_mutex_ 上。
    std::vector<std::uint8_t> pending;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        if (output_.empty()) {
            return true;
        }
        pending = output_.takeAll();
    }

    std::size_t sent = 0;
    while (sent < pending.size()) {
        const auto n = ::send(fd_, pending.data() + sent, pending.size() - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            break;
        }
        return fail(std::string("send failed: ") + std::strerror(errno));
    }

    if (sent < pending.size()) {
        // 未发完的部分放回队首，保证字节序。
        std::lock_guard<std::mutex> lock(output_mutex_);
        output_.prepend(pending.data() + sent, pending.size() - sent);
    }
    return true;
}

void TcpConnection::queueFrame(const protocol::Frame& frame) {
    std::size_t pending = 0;
    bool was_empty = false;
    {
        std::lock_guard<std::mutex> lock(output_mutex_);
        was_empty = output_.empty();
        output_.append(encoder_.encode(frame));
        pending = output_.size();
    }

    if (pending > kMaxOutputBufferSize) {
        requestClose("output buffer overflow");
        return;
    }
    if (was_empty && wakeup_callback_) {
        wakeup_callback_();
    }
}

bool TcpConnection::hasPendingOutput() const {
    std::lock_guard<std::mutex> lock(output_mutex_);
    return !output_.empty();
}

void TcpConnection::send(const protocol::Frame& frame) {
    queueFrame(frame);
}

std::shared_ptr<business::AuthSession> TcpConnection::session() const {
    return session_;
}

void TcpConnection::setPeerAddress(std::string peer_address) {
    std::lock_guard<std::mutex> lock(meta_mutex_);
    peer_address_ = std::move(peer_address);
}

std::string TcpConnection::peerAddress() const {
    std::lock_guard<std::mutex> lock(meta_mutex_);
    return peer_address_;
}

void TcpConnection::touch() {
    last_active_at_.store(static_cast<std::int64_t>(::time(nullptr)));
}

std::int64_t TcpConnection::lastActiveAt() const {
    return last_active_at_.load();
}

bool TcpConnection::idleFor(std::int64_t now_seconds, int idle_timeout_seconds) const {
    if (idle_timeout_seconds <= 0) {
        return false;
    }
    return now_seconds - last_active_at_.load() >= idle_timeout_seconds;
}

void TcpConnection::requestClose() {
    requestClose("close requested");
}

void TcpConnection::requestClose(const std::string& reason) {
    bool expected = false;
    if (close_requested_.compare_exchange_strong(expected, true)) {
        close_reason_ = reason;
        base::logWarning("connection fd=" + std::to_string(fd_) + " scheduled to close: " + reason);
        if (wakeup_callback_) {
            wakeup_callback_();
        }
    }
}

bool TcpConnection::closeRequested() const {
    return close_requested_.load();
}

const std::string& TcpConnection::closeReason() const {
    return close_reason_;
}

bool TcpConnection::fail(const std::string& reason) {
    requestClose(reason);
    return false;
}

}  // namespace net
