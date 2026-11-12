#pragma once

#include "business/User.h"

#include <cstddef>
#include <string>
#include <vector>

namespace business {

// 列表查询条件。仓储层按这个结构做过滤 + 排序 + 分页，
// 内存/文件/MySQL 三种实现共享同一套语义，避免行为漂移。
struct UserQuery {
    static constexpr int kDefaultPageSize = 10;
    static constexpr int kMaxPageSize = 100;

    int page{1};
    int page_size{kDefaultPageSize};
    std::string keyword;   // 用户名模糊匹配（大小写不敏感）
    std::string status;    // active / frozen / 空
    std::string role;      // admin / user / 空
    std::string order_by;  // id / username / status
    bool descending{false};

    void normalize();
};

struct UserPage {
    std::vector<User> users;
    int total{0};
    int page{1};
    int page_size{UserQuery::kDefaultPageSize};
};

bool userMatchesQuery(const User& user, const UserQuery& query);

// 对已取出的用户集合做过滤、排序和分页。
UserPage paginateUsers(std::vector<User> users, const UserQuery& query);

}  // namespace business
