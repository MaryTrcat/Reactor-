#include "db/MySqlConnectionPool.h"

#include <stdexcept>

namespace db {

MySqlConnectionPool::MySqlConnectionPool(MySqlConfig config, std::size_t size) : config_(config) {
    if (size == 0) {
        throw std::invalid_argument("MySQL pool size must be greater than zero");
    }

    for (std::size_t i = 0; i < size; ++i) {
        auto connection = std::make_unique<MySqlConnection>();
        std::string error;
        if (!connection->connect(config, error)) {
            throw std::runtime_error("failed to connect MySQL: " + error);
        }
        connections_.push(std::move(connection));
    }
}

std::shared_ptr<MySqlConnection> MySqlConnectionPool::acquire() {
    std::unique_ptr<MySqlConnection> connection;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        condition_.wait(lock, [this]() { return !connections_.empty(); });
        connection = std::move(connections_.front());
        connections_.pop();
    }

    // 连接闲置超过 MySQL wait_timeout 或 MySQL 重启后会失效：借出前先 ping，
    // 失效则原地重连；重连也失败就新建一条兜底，彻底失败抛异常交给上层
    // 返回 INTERNAL_ERROR，而不是把数据库故障误报成"用户名或密码错误"。
    if (!connection->ping()) {
        std::string error;
        if (!connection->connect(config_, error)) {
            auto fresh = std::make_unique<MySqlConnection>();
            if (!fresh->connect(config_, error)) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    connections_.push(std::move(connection));
                }
                condition_.notify_one();
                throw std::runtime_error("failed to reconnect MySQL: " + error);
            }
            connection = std::move(fresh);
        }
    }

    auto raw = connection.release();
    return std::shared_ptr<MySqlConnection>(raw, [this](MySqlConnection* ptr) { release(ptr); });
}

void MySqlConnectionPool::release(MySqlConnection* connection) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(std::unique_ptr<MySqlConnection>(connection));
    }
    condition_.notify_one();
}

}  // namespace db
