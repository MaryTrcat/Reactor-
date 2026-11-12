#pragma once

#include "business/AuditLogRepository.h"

#include <memory>
#include <string>

namespace business {

// 审计服务：把"谁在什么时候对谁做了什么、结果如何"落库。
//
// 审计写入失败绝不阻断业务主流程，只记录错误日志——否则存储抖动会把
// 正常请求也变成失败。
class AuditService {
public:
    explicit AuditService(std::shared_ptr<AuditLogRepository> repository = nullptr);

    void record(const std::string& actor,
                const std::string& action,
                const std::string& target,
                const std::string& result,
                const std::string& detail,
                const std::string& actor_ip);

    AuditPage list(const AuditQuery& query) const;

    bool available() const;

    void setRepository(std::shared_ptr<AuditLogRepository> repository);

private:
    std::shared_ptr<AuditLogRepository> repository_;
};

}  // namespace business
