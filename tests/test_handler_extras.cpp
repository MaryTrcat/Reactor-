#include "business/AuditService.h"
#include "business/InMemoryAuditLogRepository.h"
#include "business/InMemoryUserRepository.h"
#include "business/LoginAttemptTracker.h"
#include "business/UserHandler.h"
#include "business/UserService.h"
#include "user.pb.h"

#include <cassert>
#include <iostream>
#include <memory>

namespace {

protocol::Frame request(std::uint16_t type, std::uint32_t id, const google::protobuf::Message& message) {
    return {type, id, message.SerializeAsString()};
}

user::ListUsersResponse decodeList(const protocol::Frame& frame) {
    user::ListUsersResponse response;
    assert(response.ParseFromString(frame.body));
    return response;
}

}  // namespace

int main() {
    auto repository = std::make_shared<business::InMemoryUserRepository>();
    auto service = std::make_shared<business::UserService>(repository);
    auto audit = std::make_shared<business::AuditService>(
        std::make_shared<business::InMemoryAuditLogRepository>());

    business::LoginAttemptTracker::Config limiter_config;
    limiter_config.max_failures = 3;
    limiter_config.lock_seconds = 30;
    auto limiter = std::make_shared<business::LoginAttemptTracker>(limiter_config);

    business::UserHandler handler(service, nullptr, audit, limiter);
    auto admin = std::make_shared<business::AuthSession>();
    auto user = std::make_shared<business::AuthSession>();

    // 初始化管理员
    user::RegisterRequest admin_register;
    admin_register.set_username("root");
    admin_register.set_password("secret1");
    admin_register.set_role("admin");
    handler.handle(request(user::REGISTER_REQUEST, 1, admin_register), admin);

    user::LoginRequest admin_login;
    admin_login.set_username("root");
    admin_login.set_password("secret1");
    const auto login_frame = handler.handle(request(user::LOGIN_REQUEST, 2, admin_login), admin);
    user::LoginResponse login_response;
    assert(login_response.ParseFromString(login_frame.body));
    assert(login_response.success());
    assert(login_response.code() == user::ERROR_CODE_UNSPECIFIED);

    // 创建 12 个普通用户
    for (int i = 0; i < 12; ++i) {
        user::RegisterRequest create;
        create.set_username("staff" + std::to_string(i));
        create.set_password("secret1");
        create.set_role("user");
        const auto frame = handler.handle(request(user::REGISTER_REQUEST, 100 + i, create), admin);
        user::RegisterResponse response;
        assert(response.ParseFromString(frame.body));
        assert(response.success());
    }

    // 分页：第 2 页、每页 5 条
    user::ListUsersRequest list_request;
    list_request.set_page(2);
    list_request.set_page_size(5);
    const auto page2 = decodeList(handler.handle(request(user::LIST_USERS_REQUEST, 200, list_request), admin));
    assert(page2.success());
    assert(page2.total() == 13);
    assert(page2.users_size() == 5);
    assert(page2.page() == 2);

    // 关键字 + 角色过滤
    user::ListUsersRequest filtered_request;
    filtered_request.set_page_size(50);
    filtered_request.set_keyword("staff1");
    const auto filtered = decodeList(handler.handle(request(user::LIST_USERS_REQUEST, 201, filtered_request), admin));
    assert(filtered.total() == 3);  // staff1, staff10, staff11

    // 普通用户无权列表
    user::LoginRequest user_login;
    user_login.set_username("staff3");
    user_login.set_password("secret1");
    handler.handle(request(user::LOGIN_REQUEST, 202, user_login), user);
    const auto denied = decodeList(handler.handle(request(user::LIST_USERS_REQUEST, 203, list_request), user));
    assert(!denied.success());
    assert(denied.code() == user::PERMISSION_DENIED);

    // 心跳
    user::PingRequest ping;
    ping.set_client_time_ms(12345);
    const auto pong_frame = handler.handle(request(user::PING_REQUEST, 204, ping), user);
    assert(pong_frame.message_type == user::PONG_RESPONSE);
    user::PongResponse pong;
    assert(pong.ParseFromString(pong_frame.body));
    assert(pong.client_time_ms() == 12345);
    assert(pong.server_time_ms() > 0);

    // 改密：旧密码错误要拒绝，正确才生效
    user::ChangePasswordRequest weak_change;
    weak_change.set_username("staff3");
    weak_change.set_old_password("wrong-old");
    weak_change.set_new_password("secret2");
    const auto wrong_old_frame = handler.handle(request(user::CHANGE_PASSWORD_REQUEST, 205, weak_change), user);
    user::ChangePasswordResponse wrong_old;
    assert(wrong_old.ParseFromString(wrong_old_frame.body));
    assert(!wrong_old.success());

    user::ChangePasswordRequest good_change;
    good_change.set_username("staff3");
    good_change.set_old_password("secret1");
    good_change.set_new_password("secret2");
    const auto good_frame = handler.handle(request(user::CHANGE_PASSWORD_REQUEST, 206, good_change), user);
    user::ChangePasswordResponse good;
    assert(good.ParseFromString(good_frame.body));
    assert(good.success());

    // 审计日志：管理员可查，且能查到刚才的登录/改密记录
    user::ListAuditLogsRequest audit_request;
    audit_request.set_page_size(50);
    const auto audit_frame = handler.handle(request(user::LIST_AUDIT_LOGS_REQUEST, 207, audit_request), admin);
    user::ListAuditLogsResponse audit_response;
    assert(audit_response.ParseFromString(audit_frame.body));
    assert(audit_response.success());
    assert(audit_response.total() > 0);
    assert(!audit_response.entries(0).actor().empty());
    bool saw_password_action = false;
    for (const auto& entry : audit_response.entries()) {
        if (entry.action() == "user.change_password") {
            saw_password_action = true;
        }
    }
    assert(saw_password_action);

    // 审计日志：普通用户只能查自己的记录（请求里的 actor 被忽略），且能看到自己的改密记录
    const auto audit_denied_frame = handler.handle(request(user::LIST_AUDIT_LOGS_REQUEST, 208, audit_request), user);
    user::ListAuditLogsResponse audit_denied;
    assert(audit_denied.ParseFromString(audit_denied_frame.body));
    assert(audit_denied.success());
    bool saw_own_change = false;
    bool saw_foreign_actor = false;
    for (const auto& entry : audit_denied.entries()) {
        if (entry.actor() != "staff3") saw_foreign_actor = true;
        if (entry.action() == "user.change_password") saw_own_change = true;
    }
    assert(!saw_foreign_actor);
    assert(saw_own_change);

    // 伪装 actor=他人：服务端强制回落为自己的记录
    user::ListAuditLogsRequest spoof_request;
    spoof_request.set_page_size(50);
    spoof_request.set_actor("root");
    const auto spoof_frame = handler.handle(request(user::LIST_AUDIT_LOGS_REQUEST, 209, spoof_request), user);
    user::ListAuditLogsResponse spoof_response;
    assert(spoof_response.ParseFromString(spoof_frame.body));
    assert(spoof_response.success());
    for (const auto& entry : spoof_response.entries()) {
        assert(entry.actor() == "staff3");
    }

    // WHOAMI：返回当前会话绑定的账号信息
    const auto whoami_frame = handler.handle(request(user::WHOAMI_REQUEST, 215, user::WhoamiRequest{}), user);
    assert(whoami_frame.message_type == user::WHOAMI_RESPONSE);
    user::WhoamiResponse whoami;
    assert(whoami.ParseFromString(whoami_frame.body));
    assert(whoami.success());
    assert(whoami.username() == "staff3");
    assert(whoami.status() == "active");
    assert(whoami.server_time_seconds() > 0);

    // 登录限流：连续失败达上限后返回 TOO_MANY_ATTEMPTS
    user::LoginRequest bad_login;
    bad_login.set_username("staff4");
    bad_login.set_password("bad-password");
    auto brute_session = std::make_shared<business::AuthSession>();
    for (int i = 0; i < 3; ++i) {
        const auto frame = handler.handle(request(user::LOGIN_REQUEST, 300 + i, bad_login), brute_session);
        user::LoginResponse response;
        assert(response.ParseFromString(frame.body));
        assert(!response.success());
    }
    const auto blocked_frame = handler.handle(request(user::LOGIN_REQUEST, 310, bad_login), brute_session);
    user::LoginResponse blocked;
    assert(blocked.ParseFromString(blocked_frame.body));
    assert(!blocked.success());
    assert(blocked.code() == user::TOO_MANY_ATTEMPTS);

    // 重置密码返回一次性随机口令
    user::ResetPasswordRequest reset_request;
    reset_request.set_username("staff5");
    const auto reset_frame = handler.handle(request(user::RESET_PASSWORD_REQUEST, 320, reset_request), admin);
    assert(reset_frame.message_type == user::RESET_PASSWORD_RESPONSE);
    user::ResetPasswordResponse reset;
    assert(reset.ParseFromString(reset_frame.body));
    assert(reset.success());
    assert(!reset.generated_password().empty());
    assert(reset.generated_password() != "123456");

    user::LoginRequest reset_login;
    reset_login.set_username("staff5");
    reset_login.set_password(reset.generated_password());
    auto reset_session = std::make_shared<business::AuthSession>();
    const auto reset_login_frame = handler.handle(request(user::LOGIN_REQUEST, 321, reset_login), reset_session);
    user::LoginResponse reset_login_response;
    assert(reset_login_response.ParseFromString(reset_login_frame.body));
    assert(reset_login_response.success());

    // 冻结 / 解冻响应类型必须与消息类型一致
    user::SetUserStatusRequest freeze_request;
    freeze_request.set_username("staff6");
    freeze_request.set_frozen(true);
    const auto freeze_frame = handler.handle(request(user::SET_USER_STATUS_REQUEST, 330, freeze_request), admin);
    assert(freeze_frame.message_type == user::SET_USER_STATUS_RESPONSE);
    user::SetUserStatusResponse freeze_response;
    assert(freeze_response.ParseFromString(freeze_frame.body));
    assert(freeze_response.success());

    // 服务端运行指标：管理员可查，普通用户被拒绝
    handler.setStatsProvider([]() {
        business::ServerStats stats;
        stats.uptime_seconds = 120;
        stats.current_connections = 3;
        stats.total_connections = 10;
        stats.io_threads = 4;
        stats.worker_threads = 4;
        stats.storage = "memory";
        stats.active_sessions = 2;
        return stats;
    });

    const auto stats_frame = handler.handle(request(user::STATS_REQUEST, 340, user::StatsRequest{}), admin);
    assert(stats_frame.message_type == user::STATS_RESPONSE);
    user::StatsResponse stats_response;
    assert(stats_response.ParseFromString(stats_frame.body));
    assert(stats_response.success());
    assert(stats_response.uptime_seconds() == 120);
    assert(stats_response.current_connections() == 3);
    assert(stats_response.total_connections() == 10);
    assert(stats_response.io_threads() == 4);
    assert(stats_response.worker_threads() == 4);
    assert(stats_response.storage() == "memory");
    assert(stats_response.active_sessions() == 2);

    const auto stats_denied_frame = handler.handle(request(user::STATS_REQUEST, 341, user::StatsRequest{}), user);
    user::StatsResponse stats_denied;
    assert(stats_denied.ParseFromString(stats_denied_frame.body));
    assert(!stats_denied.success());
    assert(stats_denied.code() == user::PERMISSION_DENIED);

    // 未注入 provider 时返回内部错误而不是崩溃
    business::UserHandler bare_handler(service, nullptr, audit, limiter);
    const auto bare_frame = bare_handler.handle(request(user::STATS_REQUEST, 342, user::StatsRequest{}), admin);
    user::StatsResponse bare_response;
    assert(bare_response.ParseFromString(bare_frame.body));
    assert(!bare_response.success());
    assert(bare_response.code() == user::INTERNAL_ERROR);

    std::cout << "test_handler_extras passed\n";
    return 0;
}
