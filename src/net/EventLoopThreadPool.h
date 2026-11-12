#pragma once

#include "net/EventLoop.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace net {

// 主从 Reactor 里的 sub reactor 集合：每个线程跑一个 EventLoop。
// 主 loop 只做 accept，accept 完成后按轮询把 fd 交给 sub loop，
// 这样单线程事件循环不会被某个慢连接的读写拖住。
class EventLoopThreadPool {
public:
    explicit EventLoopThreadPool(std::size_t thread_count);
    ~EventLoopThreadPool();

    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

    void start();
    void stop();

    bool empty() const;
    std::size_t size() const;

    // 轮询取下一个 loop；空池返回 nullptr，表示退化成单 reactor 模式。
    EventLoop* nextLoop();

    // 对所有 loop 投递任务（线程安全）。
    void forEachLoop(const std::function<void(EventLoop&)>& task);

    const std::vector<std::unique_ptr<EventLoop>>& loops() const;

private:
    std::vector<std::unique_ptr<EventLoop>> loops_;
    std::vector<std::thread> threads_;
    mutable std::mutex mutex_;
    std::size_t next_{0};
    bool started_{false};
};

}  // namespace net
