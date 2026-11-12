#pragma once

#include "base/ThreadPool.h"
#include "business/UserHandler.h"
#include "net/Acceptor.h"
#include "net/EventLoop.h"
#include "net/EventLoopThreadPool.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace net {

struct TcpServerOptions {
    std::size_t worker_threads{4};
    // 0 表示退化成单 reactor：所有连接都由主 loop 处理。
    std::size_t io_threads{4};
    // 0 表示不做空闲回收。
    int idle_timeout_seconds{300};
    // 检查退出信号的周期。
    int shutdown_poll_ms{200};
};

class TcpServer {
public:
    TcpServer(const std::string& host,
              std::uint16_t port,
              std::shared_ptr<business::UserHandler> handler,
              const TcpServerOptions& options,
              std::shared_ptr<business::SessionRegistry> sessions = nullptr);

    // 阻塞运行，直到收到 SIGTERM/SIGINT 或 stop() 被调用。
    void run();
    // 线程安全，可在任意线程调用。
    void stop();

    // 线程安全的运行指标快照（STATS_REQUEST 用）。
    business::ServerStats statsSnapshot() const;

private:
    void configureLoop(EventLoop& loop);
    void dispatchNewConnection(int fd);
    void shutdown();

    Acceptor acceptor_;
    EventLoop main_loop_;
    EventLoopThreadPool io_pool_;
    base::ThreadPool workers_;
    TcpServerOptions options_;
    std::shared_ptr<business::UserHandler> handler_;
    std::shared_ptr<business::SessionRegistry> sessions_;
    std::atomic<bool> stopping_{false};
    // 累计接受的连接数（accept 成功即 +1）。
    std::atomic<std::int64_t> total_connections_{0};
    std::chrono::steady_clock::time_point started_at_{std::chrono::steady_clock::now()};
};

}  // namespace net
