#include "net/SignalHandler.h"

#include "net/EventLoop.h"

#include <csignal>

namespace net {

std::atomic<EventLoop*> SignalHandler::loop_{nullptr};
std::atomic<bool> SignalHandler::stop_requested_{false};

void SignalHandler::install(EventLoop* loop) {
    loop_.store(loop);
    stop_requested_.store(false);

    std::signal(SIGPIPE, SIG_IGN);
    std::signal(SIGINT, &SignalHandler::handle);
    std::signal(SIGTERM, &SignalHandler::handle);
}

bool SignalHandler::stopRequested() {
    return stop_requested_.load();
}

void SignalHandler::resetForTest() {
    stop_requested_.store(false);
    loop_.store(nullptr);
}

void SignalHandler::handle(int signal_number) {
    (void)signal_number;
    stop_requested_.store(true);
    if (auto* loop = loop_.load()) {
        // EventLoop::wakeup 只做一次 write(2)，满足 async-signal-safe 要求。
        loop->wakeup();
    }
}

}  // namespace net
