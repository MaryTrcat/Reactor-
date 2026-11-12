#pragma once

#include "business/AuditLogRepository.h"
#include "db/MySqlConnectionPool.h"

#include <memory>

namespace db {

class MySqlAuditLogRepository : public business::AuditLogRepository {
public:
    explicit MySqlAuditLogRepository(std::shared_ptr<MySqlConnectionPool> pool);

    bool append(business::AuditLog& log, std::string& error) override;
    business::AuditPage query(const business::AuditQuery& query) override;
    std::size_t size() const override;

private:
    std::shared_ptr<MySqlConnectionPool> pool_;
};

}  // namespace db
