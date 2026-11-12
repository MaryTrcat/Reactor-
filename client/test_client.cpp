#include "protocol/Codec.h"
#include "user.pb.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <iostream>
#include <string>

namespace {

bool sendFrame(int fd, const protocol::Frame& frame) {
    protocol::Codec codec;
    const auto bytes = codec.encode(frame);
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto sent = send(fd, bytes.data() + offset, bytes.size() - offset, 0);
        if (sent <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(sent);
    }
    return true;
}

protocol::Frame receiveFrame(int fd) {
    protocol::Codec codec;
    std::uint8_t buffer[4096];
    while (true) {
        const auto n = recv(fd, buffer, sizeof(buffer), 0);
        if (n <= 0) {
            throw std::runtime_error("connection closed");
        }
        codec.append(buffer, static_cast<std::size_t>(n));
        if (auto frame = codec.tryDecode()) {
            return *frame;
        }
    }
}

int connectToServer(const std::string& host, std::uint16_t port) {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        throw std::runtime_error("socket failed");
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) <= 0) {
        close(fd);
        throw std::runtime_error("invalid host");
    }
    if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        close(fd);
        throw std::runtime_error("connect failed");
    }
    return fd;
}

void printResponse(const protocol::Frame& frame) {
    switch (frame.message_type) {
        case user::REGISTER_RESPONSE: {
            user::RegisterResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "register: " << resp.success() << " user_id=" << resp.user_id() << " message=" << resp.message() << "\n";
            break;
        }
        case user::LOGIN_RESPONSE: {
            user::LoginResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "login: " << resp.success() << " user_id=" << resp.user_id() << " role=" << resp.role() << " message=" << resp.message() << "\n";
            break;
        }
        case user::QUERY_USER_RESPONSE: {
            user::QueryUserResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "query: " << resp.found() << " user_id=" << resp.user_id() << " username=" << resp.username() << " role=" << resp.role() << " message=" << resp.message() << "\n";
            break;
        }
        case user::CHECK_PERMISSION_RESPONSE: {
            user::CheckPermissionResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "permission: " << resp.allowed() << " message=" << resp.message() << "\n";
            break;
        }
        case user::LIST_USERS_RESPONSE: {
            user::ListUsersResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "list: " << resp.success() << " count=" << resp.users_size() << " message=" << resp.message() << "\n";
            for (const auto& user : resp.users()) {
                std::cout << "  user_id=" << user.user_id() << " username=" << user.username() << " role=" << user.role() << "\n";
            }
            break;
        }
        case user::PONG_RESPONSE: {
            user::PongResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "pong: server_time_ms=" << resp.server_time_ms()
                      << " rtt_ms=" << (resp.server_time_ms() - resp.client_time_ms()) << "\n";
            break;
        }
        case user::RESET_PASSWORD_RESPONSE: {
            user::ResetPasswordResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "reset password: " << resp.success() << " generated=" << resp.generated_password() << " message=" << resp.message() << "\n";
            break;
        }
        case user::SET_USER_STATUS_RESPONSE: {
            user::SetUserStatusResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "set status: " << resp.success() << " message=" << resp.message() << "\n";
            break;
        }
        case user::LIST_AUDIT_LOGS_RESPONSE: {
            user::ListAuditLogsResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "audit: " << resp.success() << " total=" << resp.total() << " page=" << resp.page() << "\n";
            for (const auto& entry : resp.entries()) {
                std::cout << "  [" << entry.created_at() << "] " << entry.actor() << " " << entry.action()
                          << " -> " << entry.target() << " (" << entry.result() << ") ip=" << entry.actor_ip() << "\n";
            }
            break;
        }
        case user::CHANGE_PASSWORD_RESPONSE: {
            user::ChangePasswordResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "change password: " << resp.success() << " message=" << resp.message() << "\n";
            break;
        }
        case user::DELETE_USER_RESPONSE: {
            user::DeleteUserResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "delete: " << resp.success() << " message=" << resp.message() << "\n";
            break;
        }
        case user::ERROR_RESPONSE: {
            user::ErrorResponse resp;
            resp.ParseFromString(frame.body);
            std::cout << "error: " << resp.message() << "\n";
            break;
        }
        default:
            std::cout << "unknown response type: " << frame.message_type << "\n";
            break;
    }
}

}  // namespace

int main(int argc, char** argv) {
    const std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    const auto port = static_cast<std::uint16_t>(argc > 2 ? std::stoi(argv[2]) : 9000);

    try {
        const int fd = connectToServer(host, port);
        std::uint32_t request_id = 1;

        user::RegisterRequest reg;
        reg.set_username("alice");
        reg.set_password("secret1");
        reg.set_role("admin");
        sendFrame(fd, {user::REGISTER_REQUEST, request_id++, reg.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::LoginRequest login;
        login.set_username("alice");
        login.set_password("secret1");
        sendFrame(fd, {user::LOGIN_REQUEST, request_id++, login.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::QueryUserRequest query;
        query.set_username("alice");
        sendFrame(fd, {user::QUERY_USER_REQUEST, request_id++, query.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::CheckPermissionRequest permission;
        permission.set_username("alice");
        permission.set_required_role("admin");
        sendFrame(fd, {user::CHECK_PERMISSION_REQUEST, request_id++, permission.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::ListUsersRequest list;
        list.set_page(1);
        list.set_page_size(5);
        sendFrame(fd, {user::LIST_USERS_REQUEST, request_id++, list.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::PingRequest ping;
        ping.set_client_time_ms(0);
        sendFrame(fd, {user::PING_REQUEST, request_id++, ping.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::ListAuditLogsRequest audit;
        audit.set_page_size(10);
        sendFrame(fd, {user::LIST_AUDIT_LOGS_REQUEST, request_id++, audit.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::ChangePasswordRequest change;
        change.set_username("alice");
        change.set_old_password("secret1");
        change.set_new_password("newsecret");
        sendFrame(fd, {user::CHANGE_PASSWORD_REQUEST, request_id++, change.SerializeAsString()});
        printResponse(receiveFrame(fd));

        user::DeleteUserRequest remove;
        remove.set_username("alice");
        sendFrame(fd, {user::DELETE_USER_REQUEST, request_id++, remove.SerializeAsString()});
        printResponse(receiveFrame(fd));

        close(fd);
    } catch (const std::exception& ex) {
        std::cerr << "client failed: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
