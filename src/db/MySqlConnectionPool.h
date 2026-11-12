#pragma once

#include "db/MySqlConnection.h"

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

namespace db {

class MySqlConnectionPool {
public:
    MySqlConnectionPool(MySqlConfig config, std::size_t size);

    std::shared_ptr<MySqlConnection> acquire();

private:
    void release(MySqlConnection* connection);

    MySqlConfig config_;
    std::mutex mutex_;
    std::condition_variable condition_;
    std::queue<std::unique_ptr<MySqlConnection>> connections_;
};

}  // namespace db
