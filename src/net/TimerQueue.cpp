#include "net/TimerQueue.h"

#include "base/Logger.h"
#include "net/SocketUtil.h"

#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace net {

TimerQueue::TimerQueue(int epoll_fd) : epoll_fd_(epoll_fd) {
    timer_fd_ = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (timer_fd_ < 0) {
        throw std::runtime_error("timerfd_create failed");
    }

    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = timer_fd_;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, timer_fd_, &event) < 0) {
        closeFd(timer_fd_);
        throw std::runtime_error("epoll_ctl add timerfd failed");
    }
}

TimerQueue::~TimerQueue() {
    if (epoll_fd_ >= 0 && timer_fd_ >= 0) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, timer_fd_, nullptr);
    }
    closeFd(timer_fd_);
}

int TimerQueue::fd() const {
    return timer_fd_;
}

std::int64_t TimerQueue::nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

std::uint64_t TimerQueue::addTimer(std::int64_t delay_ms,
                                   std::int64_t interval_ms,
                                   Callback callback) {
    if (!callback) {
        return kInvalidTimerId;
    }
    if (delay_ms < 0) {
        delay_ms = 0;
    }
    if (interval_ms < 0) {
        interval_ms = 0;
    }

    const auto id = next_id_++;
    const auto deadline = nowMs() + delay_ms;
    timers_.emplace(deadline, Entry{id, interval_ms, std::move(callback)});
    resetTimerFd();
    return id;
}

bool TimerQueue::cancel(std::uint64_t timer_id) {
    for (auto it = timers_.begin(); it != timers_.end(); ++it) {
        if (it->second.id == timer_id) {
            const bool was_front = (it == timers_.begin());
            timers_.erase(it);
            if (was_front) {
                resetTimerFd();
            }
            return true;
        }
    }
    return false;
}

void TimerQueue::handleExpired() {
    std::uint64_t expirations = 0;
    while (::read(timer_fd_, &expirations, sizeof(expirations)) == sizeof(expirations)) {
    }

    const auto now = nowMs();
    std::vector<Entry> due;
    while (!timers_.empty() && timers_.begin()->first <= now) {
        due.push_back(timers_.begin()->second);
        timers_.erase(timers_.begin());
    }

    for (auto& entry : due) {
        if (entry.interval_ms > 0) {
            // 重复定时器：按 interval 续期，跳过已经落后的周期，避免突发补跑。
            auto next_deadline = now + entry.interval_ms;
            timers_.emplace(next_deadline, entry);
        }
        try {
            entry.callback();
        } catch (const std::exception& ex) {
            base::logError(std::string("timer callback failed: ") + ex.what());
        }
    }

    resetTimerFd();
}

void TimerQueue::resetTimerFd() {
    itimerspec spec{};
    if (!timers_.empty()) {
        const auto deadline = timers_.begin()->first;
        const auto delta = deadline - nowMs();
        const auto delay = delta > 0 ? delta : 0;
        spec.it_value.tv_sec = delay / 1000;
        spec.it_value.tv_nsec = (delay % 1000) * 1000000;
        if (spec.it_value.tv_sec == 0 && spec.it_value.tv_nsec == 0) {
            // timerfd 要求 it_value 非 0 才会启动，至少给 1ns。
            spec.it_value.tv_nsec = 1;
        }
    }
    // interval 为 0：由我们自己在触发后重新排期，避免与业务回调打架。
    timerfd_settime(timer_fd_, 0, &spec, nullptr);
}

}  // namespace net
