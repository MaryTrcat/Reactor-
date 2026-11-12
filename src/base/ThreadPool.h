#pragma once

#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace base {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t thread_count);
    ~ThreadPool();

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // 停止接收新任务并等待已排队的任务执行完再退出（优雅关闭用）。
    void shutdown();

    template <typename Func>
    auto submit(Func&& func) -> std::future<typename std::invoke_result<Func>::type> {
        using Result = typename std::invoke_result<Func>::type;
        auto task = std::make_shared<std::packaged_task<Result()>>(std::forward<Func>(func));
        auto future = task->get_future();

        {
            std::lock_guard<std::mutex> lock(mutex_);
            tasks_.push([task]() { (*task)(); });
        }
        condition_.notify_one();
        return future;
    }

private:
    void workerLoop();

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopping_{false};
};

}  // namespace base
