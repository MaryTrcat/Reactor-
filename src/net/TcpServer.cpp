#include "net/TcpServer.h"

#include "base/Logger.h"
#include "net/SignalHandler.h"

#include <ctime>

namespace net {

TcpServer::TcpServer(const std::string& host,
                     std::uint16_t port,
                     std::shared_ptr<business::UserHandler> handler,
                     const TcpServerOptions& options,
                     std::shared_ptr<business::SessionRegistry> sessions)
    : acceptor_(host, port),
      main_loop_(acceptor_),
      io_pool_(options.io_threads),
      workers_(options.worker_threads == 0 ? 1 : options.worker_threads),
      options_(options),
      handler_(std::move(handler)),
      sessions_(sessions ? std::move(sessions) : std::make_shared<business::SessionRegistry>()) {
    configureLoop(main_loop_);
    for (auto& loop : io_pool_.loops()) {
        configureLoop(*loop);
    }

    if (io_pool_.empty()) {
        base::logWarning("io_threads=0, running in single-reactor mode");
    } else {
        main_loop_.setNewConnectionCallback([this](int fd) { dispatchNewConnection(fd); });
    }
}

void TcpServer::configureLoop(EventLoop& loop) {
    loop.setIdleTimeoutSeconds(options_.idle_timeout_seconds);

    loop.setConnectionCallback([this](const std::shared_ptr<TcpConnection>& connection) {
        connection->session()->touch(static_cast<std::int64_t>(::time(nullptr)));
        sessions_->add(std::to_string(connection->fd()), connection->session());
    });

    loop.setCloseCallback([this](int fd) { sessions_->remove(std::to_string(fd)); });

    loop.setMessageCallback([this](std::shared_ptr<TcpConnection> connection, const protocol::Frame& frame) {
        // 读事件在 loop 线程，业务处理丢到线程池，避免 DB/哈希阻塞事件循环。
        connection->session()->touch(static_cast<std::int64_t>(::time(nullptr)));
        workers_.submit([this, connection, frame]() {
            try {
                const auto response = handler_->handle(frame, connection->session(), std::to_string(connection->fd()));
                connection->queueFrame(response);
            } catch (const std::exception& ex) {
                base::logError(std::string("handler failed: ") + ex.what());
                connection->requestClose("handler exception");
            }
        });
    });
}

void TcpServer::dispatchNewConnection(int fd) {
    total_connections_.fetch_add(1, std::memory_order_relaxed);
    auto* loop = io_pool_.nextLoop();
    if (loop == nullptr) {
        main_loop_.addConnection(fd);
        return;
    }
    // fd 归属的 loop 确定后，后续所有读写都只在该线程进行；
    // 跨线程只通过 runInLoop 投递任务 + eventfd 唤醒。
    loop->runInLoop([loop, fd]() { loop->addConnection(fd); });
}

business::ServerStats TcpServer::statsSnapshot() const {
    business::ServerStats stats;
    stats.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(
                               std::chrono::steady_clock::now() - started_at_)
                               .count();
    stats.current_connections = static_cast<int>(main_loop_.connectionCount());
    for (const auto& loop : io_pool_.loops()) {
        stats.current_connections += static_cast<int>(loop->connectionCount());
    }
    stats.total_connections = total_connections_.load(std::memory_order_relaxed);
    stats.io_threads = static_cast<int>(options_.io_threads);
    stats.worker_threads = static_cast<int>(options_.worker_threads == 0 ? 1 : options_.worker_threads);
    stats.active_sessions = static_cast<int>(sessions_->list().size());
    return stats;
}

void TcpServer::run() {
    io_pool_.start();
    SignalHandler::install(&main_loop_);

    // 用定时器轮询退出标记，而不是在信号处理函数里直接动 loop 状态。
    main_loop_.runEvery(options_.shutdown_poll_ms > 0 ? options_.shutdown_poll_ms : 200, [this]() {
        if (SignalHandler::stopRequested() || stopping_.load()) {
            main_loop_.quit();
        }
    });

    base::logInfo("server started");
    main_loop_.run();
    shutdown();
}

void TcpServer::stop() {
    stopping_.store(true);
    main_loop_.wakeup();
}

void TcpServer::shutdown() {
    base::logInfo("shutting down: stop accepting, drain business tasks, close connections");

    // 1. 主 loop 已退出，不再 accept。
    // 2. 等业务线程把在途请求处理完并把响应写进连接缓冲。
    workers_.shutdown();

    // 3. sub loop 关连接（尽力 flush）后退出线程。
    io_pool_.stop();

    base::logInfo("server stopped");
}

}  // namespace net
