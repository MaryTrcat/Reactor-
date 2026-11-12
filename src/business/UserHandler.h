#pragma once

#include "business/AuditService.h"
#include "business/AuthSession.h"
#include "business/LoginAttemptTracker.h"
#include "business/SessionRegistry.h"
#include "business/UserService.h"
#include "protocol/Codec.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace business {

// 服务端运行指标快照，由 TcpServer/main 注入，UserHandler 只负责转发。
struct ServerStats {
    std::int64_t uptime_seconds{0};
    int current_connections{0};
    std::int64_t total_connections{0};
    int io_threads{0};
    int worker_threads{0};
    std::string storage{"memory"};
    int active_sessions{0};
};

// 协议 -> 业务的适配层。
//
// 这里负责三件事：解析 Protobuf、把会话身份注入业务调用、写审计日志。
// 业务规则本身留在 UserService，方便单测直接调服务层。
class UserHandler {
public:
    explicit UserHandler(std::shared_ptr<UserService> service,
                         std::shared_ptr<SessionRegistry> sessions = nullptr,
                         std::shared_ptr<AuditService> audit = nullptr,
                         std::shared_ptr<LoginAttemptTracker> login_limiter = nullptr);

    // main 在构造 TcpServer 之后注入，用于 STATS_REQUEST。
    void setStatsProvider(std::function<ServerStats()> provider);

    protocol::Frame handle(const protocol::Frame& request);
    // session_id 由网络层传入（当前实现用连接 fd 作为会话标识），
    // 用于 MY_SESSIONS_REQUEST 标记「当前设备」。
    protocol::Frame handle(const protocol::Frame& request,
                           const std::shared_ptr<AuthSession>& session,
                           const std::string& session_id = {});

private:
    protocol::Frame errorFrame(std::uint32_t request_id, const std::string& message) const;
    protocol::Frame errorFrame(std::uint32_t request_id, int code, const std::string& message) const;

    std::shared_ptr<UserService> service_;
    std::shared_ptr<SessionRegistry> sessions_;
    std::shared_ptr<AuditService> audit_;
    std::shared_ptr<LoginAttemptTracker> login_limiter_;
    std::function<ServerStats()> stats_provider_;
};

}  // namespace business
