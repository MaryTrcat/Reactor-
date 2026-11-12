#pragma once

#include "business/AuditLogRepository.h"

#include <mutex>
#include <string>
#include <vector>

namespace business {

// 追加写入的文本审计日志，供 Windows 控制台 Demo / 无 MySQL 环境使用。
//
// 查询时整文件扫描：审计日志量级远小于业务表，且这个实现只用于演示，
// 真正的生产查询走 MySqlAuditLogRepository 的索引 + 分页。
class FileAuditLogRepository : public AuditLogRepository {
public:
    explicit FileAuditLogRepository(std::string file_path);

    bool append(AuditLog& log, std::string& error) override;
    AuditPage query(const AuditQuery& query) override;
    std::size_t size() const override;

private:
    std::vector<AuditLog> loadAll() const;
    static std::string sanitize(const std::string& value);

    std::string file_path_;
    mutable std::mutex mutex_;
    std::int64_t next_id_{1};
};

}  // namespace business
