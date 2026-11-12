#include "business/FileAuditLogRepository.h"
#include "business/InMemoryAuditLogRepository.h"
#include "business/InMemoryUserRepository.h"
#include "business/UserQuery.h"
#include "business/UserService.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace {

std::shared_ptr<business::InMemoryUserRepository> makeRepository(int user_count) {
    auto repository = std::make_shared<business::InMemoryUserRepository>();
    std::int64_t id = 0;
    std::string error;
    for (int i = 0; i < user_count; ++i) {
        repository->createUser("user" + std::to_string(i), "hash", i % 5 == 0 ? "admin" : "user", id, error);
    }
    return repository;
}

void testUserPagination() {
    auto repository = std::make_shared<business::InMemoryUserRepository>();
    std::int64_t id = 0;
    std::string error;
    for (int i = 0; i < 25; ++i) {
        repository->createUser("member" + std::to_string(i), "hash", "user", id, error);
    }
    // 让其中 5 个冻结
    for (int i = 0; i < 5; ++i) {
        repository->updateStatus("member" + std::to_string(i), true, error);
    }

    business::UserQuery query;
    query.page = 2;
    query.page_size = 10;
    const auto second_page = repository->queryUsers(query);
    assert(second_page.total == 25);
    assert(second_page.page == 2);
    assert(second_page.users.size() == 10);
    assert(second_page.users.front().username == "member10");

    // 超出范围的页码返回空列表但 total 仍然正确
    query.page = 99;
    const auto empty_page = repository->queryUsers(query);
    assert(empty_page.users.empty());
    assert(empty_page.total == 25);

    // 状态过滤
    business::UserQuery frozen_query;
    frozen_query.page_size = 50;
    frozen_query.status = "frozen";
    assert(repository->queryUsers(frozen_query).total == 5);

    // 关键字过滤
    business::UserQuery keyword_query;
    keyword_query.page_size = 50;
    keyword_query.keyword = "member1";
    const auto keyword_page = repository->queryUsers(keyword_query);
    assert(keyword_page.total == 11);  // member1, member10..member19

    // 倒序
    business::UserQuery desc_query;
    desc_query.page_size = 3;
    desc_query.descending = true;
    const auto desc_page = repository->queryUsers(desc_query);
    assert(desc_page.users.front().username == "member24");

    // page_size 越界自动收敛到上限
    business::UserQuery huge_query;
    huge_query.page_size = 100000;
    const auto huge_page = repository->queryUsers(huge_query);
    assert(huge_page.page_size == business::UserQuery::kMaxPageSize);
    assert(huge_page.users.size() == 25);
}

void testServiceQueryAs() {
    auto repository = makeRepository(12);
    business::UserService service(repository);

    business::UserQuery query;
    query.page = 1;
    query.page_size = 5;

    // 非管理员不能列用户
    const auto denied = service.queryUsersAs("user1", query);
    assert(!denied.success);

    const auto allowed = service.queryUsersAs("user0", query);
    assert(allowed.success);
    assert(allowed.total == 12);
    assert(allowed.users.size() == 5);
    // 列表不得泄露密码哈希
    for (const auto& user : allowed.users) {
        assert(user.password_hash.empty());
    }
}

void testInMemoryAuditLog() {
    business::InMemoryAuditLogRepository repository(3);

    business::AuditQuery query;
    const auto empty = repository.query(query);
    assert(empty.total == 0);

    for (int i = 0; i < 5; ++i) {
        business::AuditLog log;
        log.actor = "admin";
        log.action = i % 2 == 0 ? "user.delete" : "user.list";
        log.target = "user" + std::to_string(i);
        log.result = "success";
        log.created_at = 100 + i;
        std::string error;
        assert(repository.append(log, error));
    }

    // 容量 3：只保留最近 3 条
    assert(repository.size() == 3);

    business::AuditQuery list_query;
    list_query.page_size = 10;
    const auto page = repository.query(list_query);
    assert(page.total == 3);
    assert(page.logs.front().target == "user4");  // 时间倒序

    business::AuditQuery filtered;
    filtered.action = "user.list";
    assert(repository.query(filtered).total == 1);

    business::AuditQuery ranged;
    ranged.from_timestamp = 103;
    assert(repository.query(ranged).total == 2);
}

void testFileAuditLog() {
    const auto path = std::filesystem::temp_directory_path() / "reactor_audit_test.log";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    {
        business::FileAuditLogRepository repository(path.string());
        business::AuditLog log;
        log.actor = "alice";
        log.action = "auth.login";
        log.target = "alice";
        log.result = "failed\tdetail";  // 包含分隔符，写入时必须转义
        log.actor_ip = "127.0.0.1:1000";
        std::string error;
        assert(repository.append(log, error));
        assert(log.id == 1);
        assert(log.created_at > 0);

        business::AuditQuery query;
        const auto page = repository.query(query);
        assert(page.total == 1);
        assert(page.logs.front().result.find('\t') == std::string::npos);
    }

    // 重新加载：id 自增应当从文件恢复出来的最大值继续
    {
        business::FileAuditLogRepository repository(path.string());
        business::AuditLog log;
        log.actor = "bob";
        log.action = "user.delete";
        std::string error;
        assert(repository.append(log, error));
        assert(log.id == 2);
        assert(repository.size() == 2);
    }

    std::filesystem::remove(path, ec);
}

}  // namespace

int main() {
    testUserPagination();
    testServiceQueryAs();
    testInMemoryAuditLog();
    testFileAuditLog();
    std::cout << "test_query_and_audit passed\n";
    return 0;
}
