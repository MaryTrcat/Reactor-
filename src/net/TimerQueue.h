#pragma once

#include <cstdint>
#include <functional>
#include <map>

namespace net {

// 基于 timerfd + 最小堆语义（这里用 multimap 按 deadline 排序）的定时器队列。
//
// 只允许在所属 EventLoop 线程调用。重复定时器不会为每次触发重新分配，而是在
// 触发后按 interval 重新排期，这样同一时刻大量定时器也不会产生额外对象。
class TimerQueue {
public:
    using Callback = std::function<void()>;

    static constexpr std::uint64_t kInvalidTimerId = 0;

    explicit TimerQueue(int epoll_fd);
    ~TimerQueue();

    TimerQueue(const TimerQueue&) = delete;
    TimerQueue& operator=(const TimerQueue&) = delete;

    int fd() const;

    // interval_ms 为 0 表示一次性定时器。
    std::uint64_t addTimer(std::int64_t delay_ms, std::int64_t interval_ms, Callback callback);
    bool cancel(std::uint64_t timer_id);

    // timerfd 可读时调用，执行所有到期回调。
    void handleExpired();

private:
    struct Entry {
        std::uint64_t id{0};
        std::int64_t interval_ms{0};
        Callback callback;
    };

    void resetTimerFd();
    static std::int64_t nowMs();

    int epoll_fd_{-1};
    int timer_fd_{-1};
    std::uint64_t next_id_{1};
    std::multimap<std::int64_t, Entry> timers_;
};

}  // namespace net
