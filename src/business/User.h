#pragma once

#include <cstdint>
#include <string>

namespace business {

struct User {
    std::int64_t id{};
    std::string username;
    std::string password_hash;
    std::string role;
    bool frozen{false};
};

}  // namespace business
