#pragma once

#include "net/Acceptor.h"
#include "net/TcpConnection.h"
#include "net/TimerQueue.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace net {

// 一个 EventLoop 绑定一个线程（one loop per thread）。
// 主 loop 持有 Acceptor，负责 accept 并把新 fd 分发给 sub loop；
// sub loop 只负责自己名下连接的读写与定时任务。
class EventLoop {
public:
    using MessageCallback = TcpConnection::MessageCallback;
    using ConnectionCallback = std::function<void(std::shared_ptr<TcpConnection>)>;
    using CloseCallback = std::function<void(int)>;
    using NewConnectionCallback = std::function<void(int)>;
    using Functor = std::function<void()>;

    EventLoop();
    explicit EventLoop(Acceptor& acceptor);
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void setMessageCallback(MessageCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setCloseCallback(CloseCallback callback);
    // 主 loop 使用：把 accept 到的 fd 交给别的 loop。
    // 不设置时在主 loop 内直接建立连接（单 reactor 模式）。
    void setNewConnectionCallback(NewConnectionCallback callback);

    void run();
    void stop();          // 线程安全：投递到所属线程后退出
    void quit();          // 必须在 loop 线程内调用

    bool isInLoopThread() const;
    void runInLoop(Functor functor);
    void queueInLoop(Functor functor);
    void wakeup();
    bool isRunning() const;

    std::uint64_t runEvery(std::int64_t interval_ms, Functor callback);
    std::uint64_t runAfter(std::int64_t delay_ms, Functor callback);
    void cancelTimer(std::uint64_t timer_id);

    // 必须在 loop 线程内调用。
    void addConnection(int fd);
    void closeConnection(int fd, const std::string& reason);
    void closeAllConnections(const std::string& reason);
    // 原子计数，允许监控等跨线程调用。
    std::size_t connectionCount() const;

    void setIdleTimeoutSeconds(int seconds);
    int idleTimeoutSeconds() const;

private:
    void removeConnection(int fd);
    void doPendingFunctors();
    void sweepIdleConnections();
    void updateConnectionEvents(const std::shared_ptr<TcpConnection>& connection);
    void handleAccept();

    Acceptor* acceptor_{nullptr};
    int epoll_fd_{-1};
    int wake_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread::id thread_id_;

    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    CloseCallback close_callback_;
    NewConnectionCallback new_connection_callback_;

    std::unique_ptr<TimerQueue> timer_queue_;
    std::uint64_t idle_sweep_timer_{TimerQueue::kInvalidTimerId};
    int idle_timeout_seconds_{0};

    std::unordered_map<int, std::shared_ptr<TcpConnection>> connections_;
    // 跨线程可读的存活连接数（与 connections_ 同步维护）。
    std::atomic<int> connection_count_{0};
    // 记录哪些 fd 已经挂了 EPOLLOUT，避免每次唤醒都无脑 epoll_ctl(MOD)。
    std::unordered_set<int> write_armed_;

    std::mutex pending_mutex_;
    std::vector<Functor> pending_functors_;
};

}  // namespace net
