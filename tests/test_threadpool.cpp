#include "base/ThreadPool.h"

#include <cassert>
#include <iostream>
#include <vector>

int main() {
    base::ThreadPool pool(4);
    std::vector<std::future<int>> futures;

    for (int i = 0; i < 16; ++i) {
        futures.push_back(pool.submit([i]() { return i * i; }));
    }

    int sum = 0;
    for (auto& future : futures) {
        sum += future.get();
    }

    assert(sum == 1240);
    std::cout << "test_threadpool passed\n";
    return 0;
}
