#include "db/MySqlAuditLogRepository.h"

#include <ctime>
#include <sstream>
#include <stdexcept>

namespace db {

MySqlAuditLogRepository::MySqlAuditLogRepository(std::shared_ptr<MySqlConnectionPool> pool)
    : pool_(std::move(pool)) {
    if (!pool_) {
        throw std::invalid_argument("pool must not be null");
    }
}

bool MySqlAuditLogRepository::append(business::AuditLog& log, std::string& error) {
    if (log.created_at == 0) {
        log.created_at = static_cast<std::int64_t>(::time(nullptr));
    }
    if (log.actor.empty()) {
        log.actor = "anonymous";
    }

    auto connection = pool_->acquire();
    std::ostringstream sql;
    sql << "INSERT INTO audit_logs(actor, action, target, result, detail, actor_ip, created_at) VALUES('"
        << connection->escape(log.actor) << "', '"
        << connection->escape(log.action) << "', '"
        << connection->escape(log.target) << "', '"
        << connection->escape(log.result) << "', '"
        << connection->escape(log.detail) << "', '"
        << connection->escape(log.actor_ip) << "', FROM_UNIXTIME(" << log.created_at << "))";

    if (!connection->execute(sql.str(), error)) {
        return false;
    }
    log.id = static_cast<std::int64_t>(connection->lastInsertId());
    return true;
}

business::AuditPage MySqlAuditLogRepository::query(const business::AuditQuery& raw_query) {
    business::AuditQuery query = raw_query;
    query.normalize();

    auto connection = pool_->acquire();
    std::string error;

    std::string where = " WHERE 1=1";
    if (!query.actor.empty()) {
        where += " AND actor LIKE '" + connection->escape(query.actor) + "%'";
    }
    if (!query.action.empty()) {
        where += " AND action='" + connection->escape(query.action) + "'";
    }
    if (query.from_timestamp > 0) {
        where += " AND created_at >= FROM_UNIXTIME(" + std::to_string(query.from_timestamp) + ")";
    }
    if (query.to_timestamp > 0) {
        where += " AND created_at <= FROM_UNIXTIME(" + std::to_string(query.to_timestamp) + ")";
    }

    business::AuditPage page;
    page.page = query.page;
    page.page_size = query.page_size;

    const auto count_rows = connection->query("SELECT COUNT(*) FROM audit_logs" + where, error);
    if (!count_rows.empty() && !count_rows.front().empty()) {
        try {
            page.total = std::stoi(count_rows.front().front());
        } catch (const std::exception&) {
            page.total = 0;
        }
    }

    const long long offset = static_cast<long long>(query.page - 1) * query.page_size;
    std::ostringstream sql;
    sql << "SELECT id, actor, action, target, result, detail, actor_ip, UNIX_TIMESTAMP(created_at)"
        << " FROM audit_logs" << where
        << " ORDER BY id DESC LIMIT " << query.page_size << " OFFSET " << offset;

    const auto rows = connection->query(sql.str(), error);
    page.logs.reserve(rows.size());
    for (const auto& row : rows) {
        if (row.size() < 8) {
            continue;
        }
        business::AuditLog log;
        log.id = std::stoll(row[0]);
        log.actor = row[1];
        log.action = row[2];
        log.target = row[3];
        log.result = row[4];
        log.detail = row[5];
        log.actor_ip = row[6];
        log.created_at = std::stoll(row[7]);
        page.logs.push_back(std::move(log));
    }
    return page;
}

std::size_t MySqlAuditLogRepository::size() const {
    auto connection = pool_->acquire();
    std::string error;
    const auto rows = connection->query("SELECT COUNT(*) FROM audit_logs", error);
    if (rows.empty() || rows.front().empty()) {
        return 0;
    }
    try {
        return static_cast<std::size_t>(std::stoll(rows.front().front()));
    } catch (const std::exception&) {
        return 0;
    }
}

}  // namespace db
