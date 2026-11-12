#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace business {

struct AuditLog {
    std::int64_t id{0};
    std::string actor;    // 谁操作
    std::string action;   // user.create / user.delete / auth.login ...
    std::string target;   // 操作对象
    std::string result;   // success / denied / failed
    std::string detail;   // 原始返回文案，便于排障
    std::string actor_ip;
    std::int64_t created_at{0};
};

struct AuditQuery {
    static constexpr int kDefaultPageSize = 20;
    static constexpr int kMaxPageSize = 200;

    int page{1};
    int page_size{kDefaultPageSize};
    std::string actor;
    std::string action;
    std::int64_t from_timestamp{0};
    std::int64_t to_timestamp{0};

    void normalize();
};

struct AuditPage {
    std::vector<AuditLog> logs;
    int total{0};
    int page{1};
    int page_size{AuditQuery::kDefaultPageSize};
};

// 审计日志只追加不修改，查询按时间倒序。
class AuditLogRepository {
public:
    virtual ~AuditLogRepository() = default;

    // 成功时回填 id 与 created_at。
    virtual bool append(AuditLog& log, std::string& error) = 0;
    virtual AuditPage query(const AuditQuery& query) = 0;
    virtual std::size_t size() const = 0;
};

}  // namespace business
