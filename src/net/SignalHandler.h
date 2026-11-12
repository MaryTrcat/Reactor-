#pragma once

#include <atomic>
#include <csignal>

namespace net {

class EventLoop;

// 用 self-wakeup 事件通知事件循环退出，避免在信号处理函数里做任何非
// async-signal-safe 的操作（加锁、分配内存、打日志都不安全）。
class SignalHandler {
public:
    // 只处理终止类信号；SIGPIPE 直接忽略（写已关闭连接由 MSG_NOSIGNAL 兜底）。
    static void install(EventLoop* loop);
    static bool stopRequested();
    static void resetForTest();

private:
    static void handle(int signal_number);

    static std::atomic<EventLoop*> loop_;
    static std::atomic<bool> stop_requested_;
};

}  // namespace net
