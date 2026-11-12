#pragma once

#include "business/AuditLogRepository.h"

#include <cstddef>
#include <deque>
#include <mutex>

namespace business {

// 进程内审计日志，容量固定（默认 10000 条），超出后丢弃最老的。
// 用于没有外部存储的场景和单元测试，避免内存无上限增长。
class InMemoryAuditLogRepository : public AuditLogRepository {
public:
    explicit InMemoryAuditLogRepository(std::size_t capacity = 10000);

    bool append(AuditLog& log, std::string& error) override;
    AuditPage query(const AuditQuery& query) override;
    std::size_t size() const override;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<AuditLog> logs_;
    std::int64_t next_id_{1};
};

}  // namespace business
