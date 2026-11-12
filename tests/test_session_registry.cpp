#include "business/AuthSession.h"
#include "business/SessionRegistry.h"

#include <cassert>
#include <iostream>
#include <memory>

int main() {
    business::SessionRegistry registry;
    auto admin = std::make_shared<business::AuthSession>();
    auto user = std::make_shared<business::AuthSession>();
    admin->authenticate("admin", "admin");
    user->authenticate("alice", "user");

    registry.add("10", admin);
    registry.add("11", user);
    const auto sessions = registry.list();
    assert(sessions.size() == 2);
    assert(sessions[0].session_id == "10");
    assert(sessions[0].username == "admin");
    assert(registry.forceLogout("11"));
    assert(!user->snapshot().authenticated);
    registry.remove("11");
    assert(registry.list().size() == 1);

    std::cout << "test_session_registry passed\n";
    return 0;
}
