#include "business/InMemoryAuditLogRepository.h"

#include <algorithm>
#include <ctime>

namespace business {

InMemoryAuditLogRepository::InMemoryAuditLogRepository(std::size_t capacity)
    : capacity_(capacity == 0 ? 1000 : capacity) {}

bool InMemoryAuditLogRepository::append(AuditLog& log, std::string& error) {
    if (log.actor.empty()) {
        log.actor = "anonymous";
    }
    if (log.created_at == 0) {
        log.created_at = static_cast<std::int64_t>(::time(nullptr));
    }

    std::lock_guard<std::mutex> lock(mutex_);
    log.id = next_id_++;
    logs_.push_back(log);
    while (logs_.size() > capacity_) {
        logs_.pop_front();
    }
    error.clear();
    return true;
}

AuditPage InMemoryAuditLogRepository::query(const AuditQuery& raw_query) {
    AuditQuery query = raw_query;
    query.normalize();

    std::vector<AuditLog> matched;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        matched.assign(logs_.rbegin(), logs_.rend());  // 时间倒序
    }

    std::vector<AuditLog> filtered;
    filtered.reserve(matched.size());
    for (auto& item : matched) {
        if (!query.actor.empty() && item.actor.find(query.actor) == std::string::npos) {
            continue;
        }
        if (!query.action.empty() && item.action != query.action) {
            continue;
        }
        if (query.from_timestamp > 0 && item.created_at < query.from_timestamp) {
            continue;
        }
        if (query.to_timestamp > 0 && item.created_at > query.to_timestamp) {
            continue;
        }
        filtered.push_back(std::move(item));
    }

    AuditPage page;
    page.total = static_cast<int>(filtered.size());
    page.page = query.page;
    page.page_size = query.page_size;

    const auto offset = static_cast<std::size_t>((query.page - 1) * query.page_size);
    if (offset >= filtered.size()) {
        return page;
    }
    const auto end = std::min(filtered.size(), offset + static_cast<std::size_t>(query.page_size));
    page.logs.assign(filtered.begin() + static_cast<std::ptrdiff_t>(offset),
                     filtered.begin() + static_cast<std::ptrdiff_t>(end));
    return page;
}

std::size_t InMemoryAuditLogRepository::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return logs_.size();
}

}  // namespace business
