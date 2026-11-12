#include "business/InMemoryUserRepository.h"
#include "business/UserHandler.h"
#include "business/UserService.h"
#include "protocol/Codec.h"
#include "user.pb.h"

#include <cassert>
#include <iostream>
#include <memory>

namespace {

business::UserHandler makeHandler() {
    auto repository = std::make_shared<business::InMemoryUserRepository>();
    auto service = std::make_shared<business::UserService>(repository);
    return business::UserHandler(service);
}

protocol::Frame request(std::uint16_t type, std::uint32_t id, const google::protobuf::Message& message) {
    return {type, id, message.SerializeAsString()};
}

}  // namespace

int main() {
    auto handler = makeHandler();
    auto session = std::make_shared<business::AuthSession>();

    user::RegisterRequest register_request;
    register_request.set_username("alice");
    register_request.set_password("secret1");
    register_request.set_role("admin");
    const auto register_response = handler.handle(request(user::REGISTER_REQUEST, 10, register_request));
    assert(register_response.message_type == user::REGISTER_RESPONSE);
    assert(register_response.request_id == 10);

    user::LoginRequest login_request;
    login_request.set_username("alice");
    login_request.set_password("secret1");
    const auto login_response = handler.handle(request(user::LOGIN_REQUEST, 11, login_request), session);
    assert(login_response.message_type == user::LOGIN_RESPONSE);

    user::ListUsersRequest list_request;
    const auto list_response = handler.handle(request(user::LIST_USERS_REQUEST, 12, list_request), session);
    assert(list_response.message_type == user::LIST_USERS_RESPONSE);
    assert(list_response.request_id == 12);
    user::ListUsersResponse users;
    assert(users.ParseFromString(list_response.body));
    assert(users.users_size() == 1);
    assert(users.users(0).username() == "alice");
    assert(users.users(0).GetDescriptor()->FindFieldByName("password_hash") == nullptr);

    user::ChangePasswordRequest change_request;
    change_request.set_username("alice");
    change_request.set_old_password("secret1");
    change_request.set_new_password("newsecret");
    const auto change_response = handler.handle(request(user::CHANGE_PASSWORD_REQUEST, 13, change_request), session);
    assert(change_response.message_type == user::CHANGE_PASSWORD_RESPONSE);
    assert(change_response.request_id == 13);
    user::ChangePasswordResponse changed;
    assert(changed.ParseFromString(change_response.body));
    assert(changed.success());

    user::DeleteUserRequest delete_request;
    delete_request.set_username("alice");
    const auto delete_response = handler.handle(request(user::DELETE_USER_REQUEST, 14, delete_request), session);
    assert(delete_response.message_type == user::DELETE_USER_RESPONSE);
    assert(delete_response.request_id == 14);
    user::DeleteUserResponse deleted;
    assert(deleted.ParseFromString(delete_response.body));
    assert(!deleted.success());

    auto repository = std::make_shared<business::InMemoryUserRepository>();
    auto service = std::make_shared<business::UserService>(repository);
    auto sessions = std::make_shared<business::SessionRegistry>();
    business::UserHandler online_handler(service, sessions);
    auto admin_session = std::make_shared<business::AuthSession>();
    auto user_session = std::make_shared<business::AuthSession>();
    user::RegisterRequest admin_register;
    admin_register.set_username("root");
    admin_register.set_password("secret1");
    admin_register.set_role("admin");
    online_handler.handle(request(user::REGISTER_REQUEST, 20, admin_register), admin_session);
    user::LoginRequest admin_login;
    admin_login.set_username("root");
    admin_login.set_password("secret1");
    online_handler.handle(request(user::LOGIN_REQUEST, 21, admin_login), admin_session);
    user::RegisterRequest user_register;
    user_register.set_username("bob");
    user_register.set_password("secret2");
    user_register.set_role("user");
    online_handler.handle(request(user::REGISTER_REQUEST, 22, user_register), admin_session);
    user::LoginRequest user_login;
    user_login.set_username("bob");
    user_login.set_password("secret2");
    online_handler.handle(request(user::LOGIN_REQUEST, 23, user_login), user_session);
    sessions->add("admin-session", admin_session);
    sessions->add("user-session", user_session);

    user::ListOnlineRequest online_request;
    const auto online_response = online_handler.handle(request(user::LIST_ONLINE_REQUEST, 24, online_request), admin_session);
    assert(online_response.message_type == user::LIST_ONLINE_RESPONSE);
    user::ListOnlineResponse online_users;
    assert(online_users.ParseFromString(online_response.body));
    assert(online_users.success());
    assert(online_users.sessions_size() == 2);

    user::ForceLogoutRequest force_request;
    force_request.set_session_id("user-session");
    const auto force_response = online_handler.handle(request(user::FORCE_LOGOUT_REQUEST, 25, force_request), admin_session);
    assert(force_response.message_type == user::FORCE_LOGOUT_RESPONSE);
    assert(user_session->snapshot().authenticated == false);

    std::cout << "test_user_handler passed\n";
    return 0;
}
