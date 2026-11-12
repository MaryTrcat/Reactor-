#include "business/AuditService.h"

#include "base/Logger.h"

#include <ctime>
#include <utility>

namespace business {

AuditService::AuditService(std::shared_ptr<AuditLogRepository> repository)
    : repository_(std::move(repository)) {}

void AuditService::record(const std::string& actor,
                          const std::string& action,
                          const std::string& target,
                          const std::string& result,
                          const std::string& detail,
                          const std::string& actor_ip) {
    if (!repository_) {
        return;
    }

    AuditLog log;
    log.actor = actor.empty() ? "anonymous" : actor;
    log.action = action;
    log.target = target;
    log.result = result;
    log.detail = detail;
    log.actor_ip = actor_ip;
    log.created_at = static_cast<std::int64_t>(::time(nullptr));

    std::string error;
    if (!repository_->append(log, error)) {
        base::logError("audit append failed action=" + action + ": " + error);
    }
}

AuditPage AuditService::list(const AuditQuery& query) const {
    if (!repository_) {
        AuditPage page;
        page.page = query.page;
        page.page_size = query.page_size;
        return page;
    }
    return repository_->query(query);
}

bool AuditService::available() const {
    return repository_ != nullptr;
}

void AuditService::setRepository(std::shared_ptr<AuditLogRepository> repository) {
    repository_ = std::move(repository);
}

}  // namespace business
