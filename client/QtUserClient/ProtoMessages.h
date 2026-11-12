#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

namespace qtproto {

struct LoginResponse {
    bool success{};
    qint64 userId{};
    QString role;
    QString message;
    // 登录失败后剩余可重试次数；0 表示已被限流锁定。
    int remainingAttempts{-1};
};

struct RegisterResponse {
    bool success{};
    qint64 userId{};
    QString message;
};

struct QueryUserResponse {
    bool found{};
    qint64 userId{};
    QString username;
    QString role;
    QString status;
    QString message;
};

struct CheckPermissionResponse {
    bool allowed{};
    QString message;
};

struct OperationResponse {
    bool success{};
    QString message;
};

struct ResetPasswordResponse {
    bool success{};
    QString message;
    // 服务端生成的一次性初始密码，仅成功时返回一次。
    QString generatedPassword;
};

struct UserInfo {
    qint64 userId{};
    QString username;
    QString role;
    QString status;
};

struct ListUsersResponse {
    bool success{};
    QVector<UserInfo> users;
    QString message;
    // 分页信息：total 为符合过滤条件的总数。
    int total{};
    int page{1};
    int pageSize{};
};

struct OnlineSessionInfo {
    QString sessionId;
    QString username;
    QString role;
    bool authenticated{};
    QString peerAddress;
};

struct ListOnlineResponse {
    bool success{};
    QVector<OnlineSessionInfo> sessions;
    QString message;
};

// 我的登录设备（MY_SESSIONS）：只返回本人会话。
struct MySessionInfo {
    QString sessionId;
    QString peerAddress;
    qint64 authenticatedAt{};
    qint64 lastActiveAt{};
    bool current{};
};

struct MySessionsResponse {
    bool success{};
    QString message;
    QVector<MySessionInfo> sessions;
    int total{};
};

struct PongInfo {
    qint64 clientTimeMs{};
    qint64 serverTimeMs{};
};

struct AuditLogInfo {
    qint64 id{};
    QString actor;
    QString action;
    QString target;
    QString result;
    QString detail;
    QString actorIp;
    qint64 createdAt{};
};

struct ListAuditLogsResponse {
    bool success{};
    QVector<AuditLogInfo> entries;
    QString message;
    int total{};
    int page{1};
    int pageSize{};
};

struct StatsInfo {
    bool success{};
    QString message;
    qint64 uptimeSeconds{};
    int currentConnections{};
    qint64 totalConnections{};
    int ioThreads{};
    int workerThreads{};
    QString storage;
    int activeSessions{};
};

// WHOAMI：当前会话绑定的账号信息（普通用户“我的空间”数据源）。
struct AccountInfo {
    bool success{};
    qint64 userId{};
    QString username;
    QString role;
    QString status;
    QString peerAddress;
    qint64 authenticatedAt{};
    qint64 lastActiveAt{};
    qint64 serverTimeSeconds{};
    QString message;
};

QByteArray registerRequest(const QString& username, const QString& password, const QString& role);
QByteArray loginRequest(const QString& username, const QString& password);
QByteArray queryUserRequest(const QString& username);
QByteArray checkPermissionRequest(const QString& username, const QString& role);
QByteArray changePasswordRequest(const QString& username, const QString& oldPassword, const QString& newPassword);
QByteArray deleteUserRequest(const QString& username);
QByteArray listUsersRequest(int page, int pageSize, const QString& keyword, const QString& status);
QByteArray resetPasswordRequest(const QString& username);
QByteArray setUserStatusRequest(const QString& username, bool frozen);
QByteArray listOnlineRequest();
QByteArray forceLogoutRequest(const QString& sessionId);
QByteArray pingRequest(qint64 clientTimeMs);
QByteArray listAuditLogsRequest(int page, int pageSize, const QString& actor);
QByteArray statsRequest();
QByteArray whoamiRequest();
QByteArray mySessionsRequest();

bool decodeLoginResponse(const QByteArray& bytes, LoginResponse& response);
bool decodeRegisterResponse(const QByteArray& bytes, RegisterResponse& response);
bool decodeQueryUserResponse(const QByteArray& bytes, QueryUserResponse& response);
bool decodeCheckPermissionResponse(const QByteArray& bytes, CheckPermissionResponse& response);
bool decodeOperationResponse(const QByteArray& bytes, OperationResponse& response);
bool decodeResetPasswordResponse(const QByteArray& bytes, ResetPasswordResponse& response);
bool decodeListUsersResponse(const QByteArray& bytes, ListUsersResponse& response);
bool decodeListOnlineResponse(const QByteArray& bytes, ListOnlineResponse& response);
bool decodePongResponse(const QByteArray& bytes, PongInfo& pong);
bool decodeListAuditLogsResponse(const QByteArray& bytes, ListAuditLogsResponse& response);
bool decodeStatsResponse(const QByteArray& bytes, StatsInfo& response);
bool decodeWhoamiResponse(const QByteArray& bytes, AccountInfo& response);
bool decodeMySessionsResponse(const QByteArray& bytes, MySessionsResponse& response);
bool decodeErrorResponse(const QByteArray& bytes, QString& message);

}  // namespace qtproto
