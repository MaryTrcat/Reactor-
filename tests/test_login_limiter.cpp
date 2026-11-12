#include "business/LoginAttemptTracker.h"

#include <cassert>
#include <iostream>

int main() {
    business::LoginAttemptTracker::Config config;
    config.max_failures = 3;
    config.window_seconds = 60;
    config.lock_seconds = 60;
    config.max_entries = 4;

    business::LoginAttemptTracker tracker(config);
    const auto key = business::LoginAttemptTracker::makeKey("alice", "127.0.0.1:5555");
    assert(key == "alice@127.0.0.1:5555");

    // 初始允许，剩余次数等于上限
    auto decision = tracker.check(key);
    assert(decision.allowed);
    assert(decision.remaining_attempts == 3);

    // 前两次失败后仍然允许，剩余次数递减
    decision = tracker.onFailure(key);
    assert(decision.allowed);
    assert(decision.remaining_attempts == 2);

    decision = tracker.onFailure(key);
    assert(decision.allowed);
    assert(decision.remaining_attempts == 1);

    // 第三次失败触发锁定
    decision = tracker.onFailure(key);
    assert(!decision.allowed);
    assert(decision.remaining_attempts == 0);
    assert(decision.retry_after_seconds > 0);

    // 锁定期内 check 也必须拒绝
    decision = tracker.check(key);
    assert(!decision.allowed);
    assert(!decision.reason.empty());

    // 其它 key 不受影响
    const auto other = business::LoginAttemptTracker::makeKey("bob", "127.0.0.1:5555");
    assert(tracker.check(other).allowed);

    // 成功登录清空计数
    tracker.onSuccess(key);
    decision = tracker.check(key);
    assert(decision.allowed);
    assert(decision.remaining_attempts == 3);

    // 条目上限：插入超过 max_entries 后不应无限增长
    for (int i = 0; i < 50; ++i) {
        tracker.onFailure("user" + std::to_string(i) + "@10.0.0.1");
    }
    assert(tracker.size() <= config.max_entries);

    std::cout << "test_login_limiter passed\n";
    return 0;
}
