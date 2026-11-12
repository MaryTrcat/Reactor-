#include "business/InMemoryUserRepository.h"
#include "business/UserHandler.h"
#include "business/UserService.h"
#include "user.pb.h"

#include <cassert>
#include <iostream>
#include <memory>

namespace {

protocol::Frame handle(business::UserHandler& handler,
                       const std::shared_ptr<business::AuthSession>& session,
                       std::uint16_t type,
                       std::uint32_t request_id,
                       const google::protobuf::Message& message) {
    return handler.handle({type, request_id, message.SerializeAsString()}, session);
}

}  // namespace

int main() {
    auto repository = std::make_shared<business::InMemoryUserRepository>();
    auto service = std::make_shared<business::UserService>(repository);
    business::UserHandler handler(service);
    auto session = std::make_shared<business::AuthSession>();

    user::RegisterRequest register_request;
    register_request.set_username("alice");
    register_request.set_password("secret1");
    register_request.set_role("admin");
    const auto registered = handle(handler, session, user::REGISTER_REQUEST, 1, register_request);
    assert(registered.message_type == user::REGISTER_RESPONSE);
    assert(registered.request_id == 1);

    user::LoginRequest login_request;
    login_request.set_username("alice");
    login_request.set_password("secret1");
    const auto logged_in = handle(handler, session, user::LOGIN_REQUEST, 2, login_request);
    assert(logged_in.message_type == user::LOGIN_RESPONSE);
    assert(logged_in.request_id == 2);

    user::ListUsersRequest list_request;
    const auto listed = handle(handler, session, user::LIST_USERS_REQUEST, 3, list_request);
    assert(listed.message_type == user::LIST_USERS_RESPONSE);
    assert(listed.request_id == 3);

    user::ChangePasswordRequest change_request;
    change_request.set_username("alice");
    change_request.set_new_password("newsecret");
    const auto changed = handle(handler, session, user::CHANGE_PASSWORD_REQUEST, 4, change_request);
    assert(changed.message_type == user::CHANGE_PASSWORD_RESPONSE);
    assert(changed.request_id == 4);

    user::DeleteUserRequest delete_request;
    delete_request.set_username("alice");
    const auto deleted = handle(handler, session, user::DELETE_USER_REQUEST, 5, delete_request);
    assert(deleted.message_type == user::DELETE_USER_RESPONSE);
    assert(deleted.request_id == 5);

    std::cout << "test_protocol_operations passed\n";
    return 0;
}
