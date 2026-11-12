#include "business/FileAuditLogRepository.h"

#include <algorithm>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace business {

namespace {

constexpr char kFieldSeparator = '\t';

}  // namespace

FileAuditLogRepository::FileAuditLogRepository(std::string file_path) : file_path_(std::move(file_path)) {
    if (file_path_.empty()) {
        throw std::invalid_argument("audit file path must not be empty");
    }
    const auto existing = loadAll();
    for (const auto& item : existing) {
        next_id_ = std::max(next_id_, item.id + 1);
    }
}

std::string FileAuditLogRepository::sanitize(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (const char ch : value) {
        if (ch == '\t' || ch == '\n' || ch == '\r') {
            out.push_back(' ');
            continue;
        }
        out.push_back(ch);
    }
    return out;
}

bool FileAuditLogRepository::append(AuditLog& log, std::string& error) {
    const auto path = std::filesystem::path(file_path_);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    log.id = next_id_;
    if (log.created_at == 0) {
        log.created_at = static_cast<std::int64_t>(::time(nullptr));
    }
    if (log.actor.empty()) {
        log.actor = "anonymous";
    }

    std::ofstream output(file_path_, std::ios::app);
    if (!output.is_open()) {
        error = "failed to open audit log file";
        return false;
    }

    output << log.id << kFieldSeparator
           << log.created_at << kFieldSeparator
           << sanitize(log.actor) << kFieldSeparator
           << sanitize(log.action) << kFieldSeparator
           << sanitize(log.target) << kFieldSeparator
           << sanitize(log.result) << kFieldSeparator
           << sanitize(log.actor_ip) << kFieldSeparator
           << sanitize(log.detail) << '\n';

    if (!output.good()) {
        error = "failed to write audit log file";
        return false;
    }

    ++next_id_;
    error.clear();
    return true;
}

std::vector<AuditLog> FileAuditLogRepository::loadAll() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<AuditLog> logs;
    std::ifstream input(file_path_);
    if (!input.is_open()) {
        return logs;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream row(line);
        AuditLog log;
        std::string id_text;
        std::string created_text;
        if (!std::getline(row, id_text, kFieldSeparator) ||
            !std::getline(row, created_text, kFieldSeparator) ||
            !std::getline(row, log.actor, kFieldSeparator) ||
            !std::getline(row, log.action, kFieldSeparator) ||
            !std::getline(row, log.target, kFieldSeparator) ||
            !std::getline(row, log.result, kFieldSeparator) ||
            !std::getline(row, log.actor_ip, kFieldSeparator)) {
            continue;
        }
        // detail 是最后一个字段，允许为空（此时 getline 返回 false 属正常情况）。
        std::getline(row, log.detail, kFieldSeparator);
        try {
            log.id = std::stoll(id_text);
            log.created_at = std::stoll(created_text);
        } catch (const std::exception&) {
            continue;
        }
        logs.push_back(std::move(log));
    }
    return logs;
}

AuditPage FileAuditLogRepository::query(const AuditQuery& raw_query) {
    AuditQuery query = raw_query;
    query.normalize();

    auto logs = loadAll();
    std::reverse(logs.begin(), logs.end());  // 时间倒序

    std::vector<AuditLog> filtered;
    filtered.reserve(logs.size());
    for (auto& item : logs) {
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

std::size_t FileAuditLogRepository::size() const {
    return loadAll().size();
}

}  // namespace business
