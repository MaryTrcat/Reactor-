#include "business/InMemoryUserRepository.h"
#include "business/UserService.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

int main() {
    auto repository = std::make_shared<business::InMemoryUserRepository>();
    business::UserService service(repository);

    const auto registered = service.registerUser("alice", "secret1", "admin");
    assert(registered.success);
    assert(registered.user_id == 1);

    const auto duplicate = service.registerUser("alice", "secret1", "admin");
    assert(!duplicate.success);

    // 超级管理员唯一：库里已经有 admin 时，再创建 admin 必须被拒（不落库）。
    const auto second_admin = service.registerUser("carol", "secret3", "admin");
    assert(!second_admin.success);
    assert(service.login("carol", "secret3").success == false);

    const auto login_ok = service.login("alice", "secret1");
    assert(login_ok.success);
    assert(login_ok.role == "admin");

    const auto bob_registered = service.registerUser("bob", "secret2", "user");
    assert(bob_registered.success);

    const auto admin_reset = service.adminResetPassword("alice", "bob");
    assert(admin_reset.success);
    assert(!admin_reset.generated_password.empty());
    assert(admin_reset.generated_password != "123456");
    assert(service.login("bob", admin_reset.generated_password).success);
    // 旧行为（固定重置为 123456）必须已经失效
    assert(!service.login("bob", "123456").success);

    const auto admin_freeze = service.adminSetFrozen("alice", "bob", true);
    assert(admin_freeze.success);
    assert(!service.login("bob", "123456").success);

    const auto normal_reset = service.adminResetPassword("bob", "alice");
    assert(!normal_reset.success);
    const auto normal_freeze = service.adminSetFrozen("bob", "alice", true);
    assert(!normal_freeze.success);

    const auto admin_unfreeze = service.adminSetFrozen("alice", "bob", false);
    assert(admin_unfreeze.success);
    assert(service.login("bob", admin_reset.generated_password).success);

    const auto login_bad = service.login("alice", "wrong-password");
    assert(!login_bad.success);

    const auto queried = service.queryUser("alice");
    assert(queried.found);
    assert(queried.username == "alice");

    const auto permission = service.checkPermission("alice", "admin");
    assert(permission.allowed);

    const auto users = service.listUsers();
    assert(users.size() == 2);
    assert(users[0].username == "alice");
    assert(users[1].username == "bob");
    assert(users[0].password_hash.empty());
    assert(users[1].password_hash.empty());
    assert(users[1].frozen == false);

    const auto weak_password_change = service.changePassword("alice", "new");
    assert(!weak_password_change.success);

    const auto password_changed = service.changePassword("alice", "newsecret");
    assert(password_changed.success);

    const auto old_password_login = service.login("alice", "secret1");
    assert(!old_password_login.success);

    const auto new_password_login = service.login("alice", "newsecret");
    assert(new_password_login.success);

    const auto missing_delete = service.deleteUser("missing");
    assert(!missing_delete.success);

    const auto deleted = service.deleteUser("alice");
    assert(deleted.success);

    const auto deleted_query = service.queryUser("alice");
    assert(!deleted_query.found);

    const auto deleted_login = service.login("alice", "newsecret");
    assert(!deleted_login.success);

    const auto remaining_users = service.listUsers();
    assert(remaining_users.size() == 1);
    assert(remaining_users[0].username == "bob");

    std::cout << "test_user_service passed\n";
    return 0;
}
