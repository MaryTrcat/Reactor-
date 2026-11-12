#include "business/UserQuery.h"

#include <algorithm>
#include <cctype>

namespace business {

namespace {

std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

}  // namespace

void UserQuery::normalize() {
    if (page < 1) {
        page = 1;
    }
    if (page_size <= 0) {
        page_size = kDefaultPageSize;
    }
    if (page_size > kMaxPageSize) {
        page_size = kMaxPageSize;
    }
    if (order_by.empty()) {
        order_by = "id";
    }
}

bool userMatchesQuery(const User& user, const UserQuery& query) {
    if (!query.keyword.empty()) {
        const auto needle = toLower(query.keyword);
        if (toLower(user.username).find(needle) == std::string::npos) {
            return false;
        }
    }
    if (!query.status.empty()) {
        const bool want_frozen = query.status == "frozen";
        if (want_frozen != user.frozen) {
            return false;
        }
    }
    if (!query.role.empty() && user.role != query.role) {
        return false;
    }
    return true;
}

UserPage paginateUsers(std::vector<User> users, const UserQuery& raw_query) {
    UserQuery query = raw_query;
    query.normalize();

    std::vector<User> filtered;
    filtered.reserve(users.size());
    for (auto& user : users) {
        if (userMatchesQuery(user, query)) {
            filtered.push_back(std::move(user));
        }
    }

    const auto order_by = query.order_by;
    std::sort(filtered.begin(), filtered.end(), [&](const User& left, const User& right) {
        if (order_by == "username") {
            return query.descending ? left.username > right.username : left.username < right.username;
        }
        if (order_by == "status") {
            if (left.frozen != right.frozen) {
                return query.descending ? left.frozen : !left.frozen;
            }
            return left.id < right.id;
        }
        return query.descending ? left.id > right.id : left.id < right.id;
    });

    UserPage page;
    page.total = static_cast<int>(filtered.size());
    page.page = query.page;
    page.page_size = query.page_size;

    const auto offset = static_cast<std::size_t>((query.page - 1) * query.page_size);
    if (offset >= filtered.size()) {
        return page;
    }
    const auto end = std::min(filtered.size(), offset + static_cast<std::size_t>(query.page_size));
    page.users.assign(std::make_move_iterator(filtered.begin() + static_cast<std::ptrdiff_t>(offset)),
                      std::make_move_iterator(filtered.begin() + static_cast<std::ptrdiff_t>(end)));
    return page;
}

}  // namespace business
