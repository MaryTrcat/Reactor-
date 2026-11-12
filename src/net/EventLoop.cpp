#include "net/EventLoop.h"

#include "base/Logger.h"
#include "net/SocketUtil.h"

#include <cerrno>
#include <ctime>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <unistd.h>

#include <array>
#include <stdexcept>

namespace net {

namespace {
constexpr int kMaxEvents = 256;
}

EventLoop::EventLoop() : thread_id_(std::this_thread::get_id()) {
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        throw std::runtime_error("epoll_create1 failed");
    }

    wake_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (wake_fd_ < 0) {
        closeFd(epoll_fd_);
        throw std::runtime_error("eventfd failed");
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = wake_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wake_fd_, &event) < 0) {
        closeFd(wake_fd_);
        closeFd(epoll_fd_);
        throw std::runtime_error("epoll_ctl add wake fd failed");
    }

    timer_queue_ = std::make_unique<TimerQueue>(epoll_fd_);
}

EventLoop::EventLoop(Acceptor& acceptor) : EventLoop() {
    acceptor_ = &acceptor;

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = acceptor.fd();
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, acceptor.fd(), &event) < 0) {
        throw std::runtime_error("epoll_ctl add listen fd failed");
    }
}

EventLoop::~EventLoop() {
    connections_.clear();
    timer_queue_.reset();
    closeFd(wake_fd_);
    closeFd(epoll_fd_);
}

void EventLoop::setMessageCallback(MessageCallback callback) {
    message_callback_ = std::move(callback);
}

void EventLoop::setConnectionCallback(ConnectionCallback callback) {
    connection_callback_ = std::move(callback);
}

void EventLoop::setCloseCallback(CloseCallback callback) {
    close_callback_ = std::move(callback);
}

void EventLoop::setNewConnectionCallback(NewConnectionCallback callback) {
    new_connection_callback_ = std::move(callback);
}

void EventLoop::run() {
    running_.store(true);
    thread_id_ = std::this_thread::get_id();

    if (idle_timeout_seconds_ > 0 && idle_sweep_timer_ == TimerQueue::kInvalidTimerId) {
        // 单次扫描 O(连接数)，每秒一次；相比每个连接挂一个定时器，
        // 省掉了连接活跃时更新定时器堆的开销，10k 连接量级下完全够用。
        idle_sweep_timer_ = runEvery(1000, [this]() { sweepIdleConnections(); });
    }

    std::array<epoll_event, kMaxEvents> events{};

    while (running_.load()) {
        const int count = epoll_wait(epoll_fd_, events.data(), kMaxEvents, 1000);
        if (count < 0 && errno != EINTR) {
            base::logError("epoll_wait failed");
            break;
        }

        for (int i = 0; i < count; ++i) {
            const int fd = events[i].data.fd;

            if (fd == wake_fd_) {
                std::uint64_t value = 0;
                while (::read(wake_fd_, &value, sizeof(value)) == sizeof(value)) {
                }
                doPendingFunctors();
                // 业务线程把响应写进连接缓冲后只会唤醒 loop，
                // 这里负责把"有数据待发"翻译成 EPOLLOUT。
                for (const auto& entry : connections_) {
                    updateConnectionEvents(entry.second);
                }
                continue;
            }

            if (timer_queue_ && fd == timer_queue_->fd()) {
                timer_queue_->handleExpired();
                continue;
            }

            if (acceptor_ && fd == acceptor_->fd()) {
                handleAccept();
                continue;
            }

            const auto it = connections_.find(fd);
            if (it == connections_.end()) {
                continue;
            }
            auto connection = it->second;

            if ((events[i].events & (EPOLLIN | EPOLLERR | EPOLLHUP | EPOLLRDHUP)) != 0) {
                if (!connection->handleRead()) {
                    removeConnection(fd);
                    continue;
                }
            }

            if (connection->closeRequested()) {
                removeConnection(fd);
                continue;
            }

            if ((events[i].events & EPOLLOUT) != 0) {
                if (!connection->handleWrite()) {
                    removeConnection(fd);
                    continue;
                }
            }

            if (connection->closeRequested()) {
                removeConnection(fd);
                continue;
            }

            updateConnectionEvents(connection);
        }

        doPendingFunctors();
    }

    running_.store(false);
}

void EventLoop::stop() {
    if (isInLoopThread()) {
        quit();
        return;
    }
    queueInLoop([this]() { quit(); });
}

void EventLoop::quit() {
    running_.store(false);
    wakeup();
}

bool EventLoop::isInLoopThread() const {
    return std::this_thread::get_id() == thread_id_;
}

void EventLoop::runInLoop(Functor functor) {
    if (!functor) {
        return;
    }
    if (isInLoopThread()) {
        functor();
        return;
    }
    queueInLoop(std::move(functor));
}

void EventLoop::queueInLoop(Functor functor) {
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        pending_functors_.push_back(std::move(functor));
    }
    wakeup();
}

void EventLoop::wakeup() {
    const std::uint64_t value = 1;
    const auto result = ::write(wake_fd_, &value, sizeof(value));
    (void)result;
}

bool EventLoop::isRunning() const {
    return running_.load();
}

std::uint64_t EventLoop::runEvery(std::int64_t interval_ms, Functor callback) {
    if (!timer_queue_) {
        return TimerQueue::kInvalidTimerId;
    }
    return timer_queue_->addTimer(interval_ms, interval_ms, std::move(callback));
}

std::uint64_t EventLoop::runAfter(std::int64_t delay_ms, Functor callback) {
    if (!timer_queue_) {
        return TimerQueue::kInvalidTimerId;
    }
    return timer_queue_->addTimer(delay_ms, 0, std::move(callback));
}

void EventLoop::cancelTimer(std::uint64_t timer_id) {
    if (timer_queue_) {
        timer_queue_->cancel(timer_id);
    }
}

void EventLoop::addConnection(int fd) {
    if (fd < 0) {
        return;
    }
    auto connection = std::make_shared<TcpConnection>(fd);
    connection->setMessageCallback(message_callback_);
    connection->setCloseCallback([this](int closed_fd) { removeConnection(closed_fd); });
    connection->setWakeupCallback([this]() { wakeup(); });
    connection->touch();
    connections_[fd] = connection;

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) {
        connections_.erase(fd);
        closeFd(fd);
        return;
    }
    connection_count_.fetch_add(1, std::memory_order_relaxed);

    if (connection_callback_) {
        connection_callback_(connection);
    }
    base::logInfo("client connected fd=" + std::to_string(fd) + " peer=" + connection->peerAddress());
}

void EventLoop::closeConnection(int fd, const std::string& reason) {
    const auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    it->second->requestClose(reason);
    // 关连接前尽力把已排队的响应刷出去，减少"服务端已处理但客户端没收到"的情况。
    it->second->handleWrite();
    removeConnection(fd);
}

void EventLoop::closeAllConnections(const std::string& reason) {
    std::vector<int> fds;
    fds.reserve(connections_.size());
    for (const auto& entry : connections_) {
        fds.push_back(entry.first);
    }
    for (const int fd : fds) {
        closeConnection(fd, reason);
    }
}

std::size_t EventLoop::connectionCount() const {
    return static_cast<std::size_t>(connection_count_.load(std::memory_order_relaxed));
}

void EventLoop::setIdleTimeoutSeconds(int seconds) {
    idle_timeout_seconds_ = seconds < 0 ? 0 : seconds;
}

int EventLoop::idleTimeoutSeconds() const {
    return idle_timeout_seconds_;
}

void EventLoop::removeConnection(int fd) {
    const auto it = connections_.find(fd);
    if (it == connections_.end()) {
        return;
    }
    // 先摘掉容器再销毁，避免 TcpConnection 析构期间回调重入导致迭代器失效。
    auto connection = it->second;
    connections_.erase(it);
    connection_count_.fetch_sub(1, std::memory_order_relaxed);
    write_armed_.erase(fd);
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
    if (close_callback_) {
        close_callback_(fd);
    }
    base::logInfo("client closed fd=" + std::to_string(fd) + " reason=" + connection->closeReason());
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    {
        std::lock_guard<std::mutex> lock(pending_mutex_);
        functors.swap(pending_functors_);
    }
    for (auto& functor : functors) {
        try {
            functor();
        } catch (const std::exception& ex) {
            base::logError(std::string("pending functor failed: ") + ex.what());
        }
    }
}

void EventLoop::sweepIdleConnections() {
    if (idle_timeout_seconds_ <= 0) {
        return;
    }
    const auto now = static_cast<std::int64_t>(::time(nullptr));
    std::vector<int> idle_fds;
    for (const auto& entry : connections_) {
        if (entry.second->idleFor(now, idle_timeout_seconds_)) {
            idle_fds.push_back(entry.first);
        }
    }
    for (const int fd : idle_fds) {
        base::logWarning("closing idle connection fd=" + std::to_string(fd));
        closeConnection(fd, "idle timeout");
    }
}

void EventLoop::updateConnectionEvents(const std::shared_ptr<TcpConnection>& connection) {
    const int fd = connection->fd();
    const bool want_write = connection->hasPendingOutput();
    const bool armed = write_armed_.count(fd) > 0;
    if (want_write == armed) {
        return;
    }

    epoll_event event{};
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if (want_write) {
        event.events |= EPOLLOUT;
    }
    event.data.fd = fd;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) != 0) {
        return;
    }
    if (want_write) {
        write_armed_.insert(fd);
    } else {
        write_armed_.erase(fd);
    }
}

void EventLoop::handleAccept() {
    if (!acceptor_) {
        return;
    }
    while (true) {
        const int client_fd = acceptor_->acceptClient();
        if (client_fd < 0) {
            break;
        }
        if (new_connection_callback_) {
            new_connection_callback_(client_fd);
        } else {
            addConnection(client_fd);
        }
    }
}

}  // namespace net
