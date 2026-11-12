#include "db/MySqlConnection.h"

#include <stdexcept>

namespace db {

MySqlConnection::MySqlConnection() {
    handle_ = mysql_init(nullptr);
    if (!handle_) {
        throw std::runtime_error("mysql_init failed");
    }
}

MySqlConnection::~MySqlConnection() {
    if (handle_) {
        mysql_close(handle_);
    }
}

bool MySqlConnection::connect(const MySqlConfig& config, std::string& error) {
    if (!mysql_real_connect(handle_,
                            config.host.c_str(),
                            config.user.c_str(),
                            config.password.c_str(),
                            config.database.c_str(),
                            config.port,
                            nullptr,
                            0)) {
        error = mysql_error(handle_);
        return false;
    }
    mysql_set_character_set(handle_, "utf8mb4");
    return true;
}

bool MySqlConnection::ping() {
    return handle_ != nullptr && mysql_ping(handle_) == 0;
}

bool MySqlConnection::execute(const std::string& sql, std::string& error) {
    if (mysql_real_query(handle_, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0) {
        error = mysql_error(handle_);
        return false;
    }
    return true;
}

std::vector<std::vector<std::string>> MySqlConnection::query(const std::string& sql, std::string& error) {
    std::vector<std::vector<std::string>> rows;
    if (!execute(sql, error)) {
        return rows;
    }

    MYSQL_RES* result = mysql_store_result(handle_);
    if (!result) {
        if (mysql_field_count(handle_) != 0) {
            error = mysql_error(handle_);
        }
        return rows;
    }

    const auto field_count = mysql_num_fields(result);
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result)) != nullptr) {
        unsigned long* lengths = mysql_fetch_lengths(result);
        std::vector<std::string> values;
        values.reserve(field_count);
        for (unsigned int i = 0; i < field_count; ++i) {
            values.emplace_back(row[i] ? std::string(row[i], lengths[i]) : "");
        }
        rows.push_back(std::move(values));
    }

    mysql_free_result(result);
    return rows;
}

std::uint64_t MySqlConnection::lastInsertId() const {
    return mysql_insert_id(handle_);
}

std::uint64_t MySqlConnection::affectedRows() const {
    return mysql_affected_rows(handle_);
}

std::string MySqlConnection::escape(const std::string& value) {
    std::string escaped;
    escaped.resize(value.size() * 2 + 1);
    const auto length = mysql_real_escape_string(handle_, escaped.data(), value.c_str(), static_cast<unsigned long>(value.size()));
    escaped.resize(length);
    return escaped;
}

}  // namespace db
