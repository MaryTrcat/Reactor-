#include "net/EventLoopThreadPool.h"

#include "base/Logger.h"

#include <chrono>
#include <string>

namespace net {

EventLoopThreadPool::EventLoopThreadPool(std::size_t thread_count) {
    loops_.reserve(thread_count);
    for (std::size_t i = 0; i < thread_count; ++i) {
        loops_.push_back(std::make_unique<EventLoop>());
    }
}

EventLoopThreadPool::~EventLoopThreadPool() {
    stop();
}

void EventLoopThreadPool::start() {
    if (started_) {
        return;
    }
    started_ = true;

    threads_.reserve(loops_.size());
    for (auto& loop : loops_) {
        threads_.emplace_back([loop = loop.get()]() { loop->run(); });
    }

    // 等所有 loop 真正跑起来再返回：否则 accept 后立刻 runInLoop 会落到
    // 还没设置 thread_id 的 loop 上，任务虽然不会丢，但会让运行状态难以观察。
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (auto& loop : loops_) {
        while (!loop->isRunning() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::yield();
        }
    }
}

void EventLoopThreadPool::stop() {
    if (!started_) {
        return;
    }
    for (auto& loop : loops_) {
        loop->runInLoop([raw = loop.get()]() {
            // 退出前先把连接关掉，让客户端立刻感知而不是等 TCP 超时。
            raw->closeAllConnections("server shutdown");
            raw->quit();
        });
    }
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    threads_.clear();
    started_ = false;
    base::logInfo("io thread pool stopped");
}

bool EventLoopThreadPool::empty() const {
    return loops_.empty();
}

std::size_t EventLoopThreadPool::size() const {
    return loops_.size();
}

EventLoop* EventLoopThreadPool::nextLoop() {
    if (loops_.empty()) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto* loop = loops_[next_ % loops_.size()].get();
    ++next_;
    return loop;
}

void EventLoopThreadPool::forEachLoop(const std::function<void(EventLoop&)>& task) {
    if (!task) {
        return;
    }
    for (auto& loop : loops_) {
        loop->runInLoop([raw = loop.get(), task]() { task(*raw); });
    }
}

const std::vector<std::unique_ptr<EventLoop>>& EventLoopThreadPool::loops() const {
    return loops_;
}

}  // namespace net
