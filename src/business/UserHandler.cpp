#include "business/UserHandler.h"

#include "base/Logger.h"
#include "user.pb.h"

#include <chrono>
#include <ctime>
#include <stdexcept>

namespace business {

namespace {

constexpr const char* kActionRegister = "auth.register";
constexpr const char* kActionLogin = "auth.login";
constexpr const char* kActionChangePassword = "user.change_password";
constexpr const char* kActionResetPassword = "user.reset_password";
constexpr const char* kActionFreeze = "user.freeze";
constexpr const char* kActionUnfreeze = "user.unfreeze";
constexpr const char* kActionDelete = "user.delete";
constexpr const char* kActionListUsers = "user.list";
constexpr const char* kActionListAudit = "audit.list";
constexpr const char* kActionForceLogout = "session.force_logout";
constexpr const char* kActionStats = "server.stats";

user::ErrorCode mapErrorCode(const std::string& message) {
    if (message.find("authentication required") != std::string::npos) {
        return user::UNAUTHENTICATED;
    }
    if (message.find("permission") != std::string::npos) {
        return user::PERMISSION_DENIED;
    }
    if (message.find("not found") != std::string::npos) {
        return user::NOT_FOUND;
    }
    if (message.find("already exists") != std::string::npos) {
        return user::CONFLICT;
    }
    if (message.find("only one super admin") != std::string::npos) {
        return user::CONFLICT;
    }
    if (message.find("too many") != std::string::npos) {
        return user::TOO_MANY_ATTEMPTS;
    }
    if (message.find("frozen") != std::string::npos) {
        return user::PERMISSION_DENIED;
    }
    return user::BAD_REQUEST;
}

std::int64_t nowMillis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

UserHandler::UserHandler(std::shared_ptr<UserService> service,
                         std::shared_ptr<SessionRegistry> sessions,
                         std::shared_ptr<AuditService> audit,
                         std::shared_ptr<LoginAttemptTracker> login_limiter)
    : service_(std::move(service)),
      sessions_(std::move(sessions)),
      audit_(std::move(audit)),
      login_limiter_(std::move(login_limiter)) {
    if (!service_) {
        throw std::invalid_argument("service must not be null");
    }
    if (!sessions_) {
        sessions_ = std::make_shared<SessionRegistry>();
    }
    if (!audit_) {
        audit_ = std::make_shared<AuditService>();
    }
    if (!login_limiter_) {
        login_limiter_ = std::make_shared<LoginAttemptTracker>();
    }
}

protocol::Frame UserHandler::handle(const protocol::Frame& request) {
    return handle(request, std::make_shared<AuthSession>(), {});
}

void UserHandler::setStatsProvider(std::function<ServerStats()> provider) {
    stats_provider_ = std::move(provider);
}

protocol::Frame UserHandler::handle(const protocol::Frame& request,
                                    const std::shared_ptr<AuthSession>& session,
                                    const std::string& session_id) {
    try {
        if (!session) {
            return errorFrame(request.request_id, user::UNAUTHENTICATED, "authentication session unavailable");
        }
        const auto auth = session->snapshot();

        switch (request.message_type) {
            case user::REGISTER_REQUEST: {
                user::RegisterRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid register request");
                }
                const auto result = service_->registerUserAs(auth.authenticated ? auth.username : "",
                                                              req.username(), req.password(), req.role());
                audit_->record(auth.authenticated ? auth.username : "anonymous",
                               kActionRegister,
                               req.username(),
                               result.success ? "success" : "failed",
                               result.message,
                               auth.peer_address);
                user::RegisterResponse resp;
                resp.set_success(result.success);
                resp.set_user_id(result.user_id);
                resp.set_message(result.message);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::REGISTER_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::LOGIN_REQUEST: {
                user::LoginRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid login request");
                }

                // 限流在业务校验之前：被锁定时连密码哈希都不算，直接快速失败。
                const auto key = LoginAttemptTracker::makeKey(req.username(), auth.peer_address);
                const auto allowed = login_limiter_->check(key);
                if (!allowed.allowed) {
                    audit_->record(req.username(), kActionLogin, req.username(), "blocked",
                                   allowed.reason, auth.peer_address);
                    user::LoginResponse resp;
                    resp.set_success(false);
                    resp.set_message("too many failed attempts, retry later");
                    resp.set_code(user::TOO_MANY_ATTEMPTS);
                    resp.set_remaining_attempts(0);
                    return {user::LOGIN_RESPONSE, request.request_id, resp.SerializeAsString()};
                }

                const auto result = service_->login(req.username(), req.password());
                int remaining = allowed.remaining_attempts;
                if (result.success) {
                    login_limiter_->onSuccess(key);
                } else {
                    const auto failure = login_limiter_->onFailure(key);
                    remaining = failure.remaining_attempts;
                    if (!failure.allowed) {
                        remaining = 0;
                    }
                }

                audit_->record(req.username(), kActionLogin, req.username(),
                               result.success ? "success" : "failed", result.message, auth.peer_address);

                user::LoginResponse resp;
                resp.set_success(result.success);
                resp.set_user_id(result.user_id);
                resp.set_role(result.role);
                resp.set_message(result.message);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                resp.set_remaining_attempts(remaining);
                if (result.success) {
                    session->authenticate(req.username(), result.role,
                                          static_cast<std::int64_t>(::time(nullptr)));
                }
                return {user::LOGIN_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::QUERY_USER_REQUEST: {
                user::QueryUserRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid query request");
                }
                const auto result = service_->queryUserAs(auth.authenticated ? auth.username : "", req.username());
                user::QueryUserResponse resp;
                resp.set_found(result.found);
                resp.set_user_id(result.user_id);
                resp.set_username(result.username);
                resp.set_role(result.role);
                resp.set_message(result.message);
                resp.set_status(result.frozen ? "frozen" : "active");
                resp.set_code(result.found ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::QUERY_USER_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::CHECK_PERMISSION_REQUEST: {
                user::CheckPermissionRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid permission request");
                }
                const auto result = service_->checkPermission(auth.authenticated ? auth.username : "", req.required_role());
                user::CheckPermissionResponse resp;
                resp.set_allowed(result.allowed);
                resp.set_message(result.message);
                resp.set_code(result.allowed ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::CHECK_PERMISSION_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::CHANGE_PASSWORD_REQUEST: {
                user::ChangePasswordRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid change-password request");
                }
                if (!auth.authenticated || req.username() != auth.username) {
                    return errorFrame(request.request_id, user::PERMISSION_DENIED,
                                      "users can only change their own password");
                }
                const auto result = service_->changeOwnPassword(auth.username, req.old_password(), req.new_password());
                audit_->record(auth.username, kActionChangePassword, auth.username,
                               result.success ? "success" : "failed", result.message, auth.peer_address);
                user::ChangePasswordResponse resp;
                resp.set_success(result.success);
                resp.set_message(result.message);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::CHANGE_PASSWORD_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::DELETE_USER_REQUEST: {
                user::DeleteUserRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid delete-user request");
                }
                const auto result = service_->adminDeleteUser(auth.authenticated ? auth.username : "", req.username());
                audit_->record(auth.username, kActionDelete, req.username(),
                               result.success ? "success" : "denied", result.message, auth.peer_address);
                user::DeleteUserResponse resp;
                resp.set_success(result.success);
                resp.set_message(result.message);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::DELETE_USER_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::LIST_USERS_REQUEST: {
                user::ListUsersRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid list-users request");
                }
                UserQuery query;
                query.page = req.page() > 0 ? req.page() : 1;
                query.page_size = req.page_size();
                query.keyword = req.keyword();
                query.status = req.status();
                query.role = req.role();
                query.order_by = req.order_by();
                query.descending = req.descending();

                const auto result = service_->queryUsersAs(auth.authenticated ? auth.username : "", query);
                user::ListUsersResponse resp;
                resp.set_success(result.success);
                resp.set_message(result.message);
                resp.set_total(result.total);
                resp.set_page(result.page);
                resp.set_page_size(result.page_size);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                for (const auto& item : result.users) {
                    auto* info = resp.add_users();
                    info->set_user_id(item.id);
                    info->set_username(item.username);
                    info->set_role(item.role);
                    info->set_status(item.frozen ? "frozen" : "active");
                }
                if (result.success) {
                    audit_->record(auth.username, kActionListUsers, "", "success",
                                   "page=" + std::to_string(result.page) + " total=" + std::to_string(result.total),
                                   auth.peer_address);
                }
                return {user::LIST_USERS_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::RESET_PASSWORD_REQUEST: {
                user::ResetPasswordRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid reset-password request");
                }
                const auto result = service_->adminResetPassword(auth.authenticated ? auth.username : "", req.username());
                audit_->record(auth.username, kActionResetPassword, req.username(),
                               result.success ? "success" : "denied", result.message, auth.peer_address);
                user::ResetPasswordResponse resp;
                resp.set_success(result.success);
                resp.set_message(result.message);
                resp.set_generated_password(result.generated_password);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::RESET_PASSWORD_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::SET_USER_STATUS_REQUEST: {
                user::SetUserStatusRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid account-status request");
                }
                const auto result = service_->adminSetFrozen(auth.authenticated ? auth.username : "",
                                                              req.username(), req.frozen());
                audit_->record(auth.username, req.frozen() ? kActionFreeze : kActionUnfreeze, req.username(),
                               result.success ? "success" : "denied", result.message, auth.peer_address);
                user::SetUserStatusResponse resp;
                resp.set_success(result.success);
                resp.set_message(result.message);
                resp.set_code(result.success ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(result.message));
                return {user::SET_USER_STATUS_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::LIST_ONLINE_REQUEST: {
                user::ListOnlineRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid online-session request");
                }
                const auto permission = service_->checkAdminPermission(auth.authenticated ? auth.username : "");
                user::ListOnlineResponse resp;
                resp.set_success(permission.allowed);
                resp.set_message(permission.message);
                resp.set_code(permission.allowed ? user::ERROR_CODE_UNSPECIFIED : mapErrorCode(permission.message));
                if (permission.allowed) {
                    for (const auto& item : sessions_->list()) {
                        auto* info = resp.add_sessions();
                        info->set_session_id(item.session_id);
                        info->set_username(item.username);
                        info->set_role(item.role);
                        info->set_authenticated(item.authenticated);
                        info->set_peer_address(item.peer_address);
                        info->set_last_active_at(item.last_active_at);
                    }
                }
                return {user::LIST_ONLINE_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::FORCE_LOGOUT_REQUEST: {
                user::ForceLogoutRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid force-logout request");
                }
                const auto permission = service_->checkAdminPermission(auth.authenticated ? auth.username : "");
                user::ForceLogoutResponse resp;
                // 管理员可以下线任意会话；普通用户只能下线自己的其他会话（自助退出其他设备）。
                bool ownedByCaller = false;
                for (const auto& item : sessions_->list()) {
                    if (item.session_id == req.session_id()) {
                        ownedByCaller = auth.authenticated && item.authenticated && item.username == auth.username;
                        break;
                    }
                }
                const bool allowed = permission.allowed || ownedByCaller;
                if (!allowed) {
                    const auto message = auth.authenticated ? permission.message : std::string("authentication required");
                    resp.set_success(false);
                    resp.set_message(message);
                    resp.set_code(mapErrorCode(message));
                } else if (req.session_id() == session_id) {
                    // 不允许把自己这一条踢掉，否则界面会突然断线又没提示。
                    resp.set_success(false);
                    resp.set_message("cannot log out the current session");
                    resp.set_code(user::CONFLICT);
                } else if (sessions_->forceLogout(req.session_id())) {
                    resp.set_success(true);
                    resp.set_message("session logged out");
                    audit_->record(auth.username, kActionForceLogout, req.session_id(), "success",
                                   permission.allowed ? "session logged out" : "self logout other device",
                                   auth.peer_address);
                } else {
                    resp.set_success(false);
                    resp.set_message("session not found");
                    resp.set_code(user::NOT_FOUND);
                }
                return {user::FORCE_LOGOUT_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::MY_SESSIONS_REQUEST: {
                user::MySessionsResponse resp;
                if (!auth.authenticated) {
                    resp.set_success(false);
                    resp.set_message("authentication required");
                    resp.set_code(user::UNAUTHENTICATED);
                    return {user::MY_SESSIONS_RESPONSE, request.request_id, resp.SerializeAsString()};
                }
                int total = 0;
                for (const auto& item : sessions_->list()) {
                    if (!item.authenticated || item.username != auth.username) continue;
                    auto* info = resp.add_sessions();
                    info->set_session_id(item.session_id);
                    info->set_peer_address(item.peer_address);
                    info->set_authenticated_at(item.authenticated_at);
                    info->set_last_active_at(item.last_active_at);
                    info->set_current(!session_id.empty() && item.session_id == session_id);
                    ++total;
                }
                resp.set_total(total);
                resp.set_success(true);
                resp.set_message("ok");
                return {user::MY_SESSIONS_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::PING_REQUEST: {
                user::PingRequest req;
                user::PongResponse resp;
                if (req.ParseFromString(request.body)) {
                    resp.set_client_time_ms(req.client_time_ms());
                }
                resp.set_server_time_ms(nowMillis());
                return {user::PONG_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::STATS_REQUEST: {
                const auto permission = service_->checkAdminPermission(auth.authenticated ? auth.username : "");
                user::StatsResponse resp;
                if (!permission.allowed) {
                    resp.set_success(false);
                    resp.set_message(permission.message);
                    resp.set_code(mapErrorCode(permission.message));
                    return {user::STATS_RESPONSE, request.request_id, resp.SerializeAsString()};
                }
                if (stats_provider_) {
                    const auto stats = stats_provider_();
                    resp.set_uptime_seconds(stats.uptime_seconds);
                    resp.set_current_connections(stats.current_connections);
                    resp.set_total_connections(stats.total_connections);
                    resp.set_io_threads(stats.io_threads);
                    resp.set_worker_threads(stats.worker_threads);
                    resp.set_storage(stats.storage);
                    resp.set_active_sessions(stats.active_sessions);
                } else {
                    resp.set_success(false);
                    resp.set_message("stats provider unavailable");
                    resp.set_code(user::INTERNAL_ERROR);
                    return {user::STATS_RESPONSE, request.request_id, resp.SerializeAsString()};
                }
                resp.set_success(true);
                resp.set_message("server stats");
                audit_->record(auth.username, kActionStats, "", "success", "", auth.peer_address);
                return {user::STATS_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::WHOAMI_REQUEST: {
                user::WhoamiResponse resp;
                if (!auth.authenticated) {
                    resp.set_success(false);
                    resp.set_message("authentication required");
                    resp.set_code(user::UNAUTHENTICATED);
                    return {user::WHOAMI_RESPONSE, request.request_id, resp.SerializeAsString()};
                }
                const auto profile = service_->queryUser(auth.username);
                resp.set_success(profile.found);
                if (!profile.found) {
                    // 会话有效但账号已被删除：提示客户端重新登录。
                    resp.set_message("account no longer exists");
                    resp.set_code(user::NOT_FOUND);
                    return {user::WHOAMI_RESPONSE, request.request_id, resp.SerializeAsString()};
                }
                resp.set_user_id(profile.user_id);
                resp.set_username(profile.username);
                resp.set_role(profile.role);
                resp.set_status(profile.frozen ? "frozen" : "active");
                resp.set_peer_address(auth.peer_address);
                resp.set_authenticated_at(auth.authenticated_at);
                resp.set_last_active_at(auth.last_active_at);
                resp.set_server_time_seconds(static_cast<std::int64_t>(::time(nullptr)));
                return {user::WHOAMI_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            case user::LIST_AUDIT_LOGS_REQUEST: {
                user::ListAuditLogsRequest req;
                if (!req.ParseFromString(request.body)) {
                    return errorFrame(request.request_id, user::BAD_REQUEST, "invalid audit-log request");
                }
                user::ListAuditLogsResponse resp;
                if (!auth.authenticated) {
                    resp.set_success(false);
                    resp.set_message("authentication required");
                    resp.set_code(user::UNAUTHENTICATED);
                    return {user::LIST_AUDIT_LOGS_RESPONSE, request.request_id, resp.SerializeAsString()};
                }
                // 管理员可查所有人；普通用户只允许查自己的记录，请求里的 actor 不可信。
                const bool admin = service_->checkAdminPermission(auth.username).allowed;
                if (!audit_->available()) {
                    resp.set_success(false);
                    resp.set_message("audit log is not configured");
                    resp.set_code(user::INTERNAL_ERROR);
                    return {user::LIST_AUDIT_LOGS_RESPONSE, request.request_id, resp.SerializeAsString()};
                }

                AuditQuery query;
                query.page = req.page() > 0 ? req.page() : 1;
                query.page_size = req.page_size();
                query.actor = admin ? req.actor() : auth.username;
                query.action = req.action();
                query.from_timestamp = req.from_timestamp();
                query.to_timestamp = req.to_timestamp();

                const auto page = audit_->list(query);
                resp.set_success(true);
                resp.set_message("audit logs listed");
                resp.set_total(page.total);
                resp.set_page(page.page);
                resp.set_page_size(page.page_size);
                for (const auto& item : page.logs) {
                    auto* entry = resp.add_entries();
                    entry->set_id(item.id);
                    entry->set_actor(item.actor);
                    entry->set_action(item.action);
                    entry->set_target(item.target);
                    entry->set_result(item.result);
                    entry->set_detail(item.detail);
                    entry->set_actor_ip(item.actor_ip);
                    entry->set_created_at(item.created_at);
                }
                return {user::LIST_AUDIT_LOGS_RESPONSE, request.request_id, resp.SerializeAsString()};
            }
            default:
                return errorFrame(request.request_id, user::BAD_REQUEST, "unknown message type");
        }
    } catch (const std::exception& ex) {
        base::logError(std::string("handler exception: ") + ex.what());
        return errorFrame(request.request_id, user::INTERNAL_ERROR, ex.what());
    }
}

protocol::Frame UserHandler::errorFrame(std::uint32_t request_id, const std::string& message) const {
    return errorFrame(request_id, mapErrorCode(message), message);
}

protocol::Frame UserHandler::errorFrame(std::uint32_t request_id, int code, const std::string& message) const {
    user::ErrorResponse resp;
    resp.set_message(message);
    resp.set_code(static_cast<user::ErrorCode>(code));
    return {user::ERROR_RESPONSE, request_id, resp.SerializeAsString()};
}

}  // namespace business
