#pragma once

#include <mysql.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace db {

struct MySqlConfig {
    std::string host{"127.0.0.1"};
    unsigned int port{3306};
    std::string user{"root"};
    std::string password;
    std::string database{"reactor_user_service"};
};

class MySqlConnection {
public:
    MySqlConnection();
    ~MySqlConnection();

    MySqlConnection(const MySqlConnection&) = delete;
    MySqlConnection& operator=(const MySqlConnection&) = delete;

    bool connect(const MySqlConfig& config, std::string& error);
    // 心跳检查连接是否仍可用（mysql_ping），失效时由调用方负责重连。
    bool ping();
    bool execute(const std::string& sql, std::string& error);
    std::vector<std::vector<std::string>> query(const std::string& sql, std::string& error);
    std::uint64_t lastInsertId() const;
    std::uint64_t affectedRows() const;

    std::string escape(const std::string& value);

private:
    MYSQL* handle_{nullptr};
};

}  // namespace db
