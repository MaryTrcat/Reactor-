#pragma once

#include "ClientSession.h"
#include "DashboardWidget.h"

#include <QByteArray>
#include <QMainWindow>
#include <QStringList>
#include <QVector>

#include <cstdint>

struct UserRow;

class QStackedWidget;
class QThread;
class QTimer;
class LoginWidget;
class RegisterWidget;
class QtTcpClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void autoLogin(const QString& username, const QString& password);

private slots:
    void onLoginRequested(const QString& username, const QString& password);
    void onConnected();
    void onDisconnected();
    void onFrameReceived(quint16 messageType, quint32 requestId, const QByteArray& body);
    void onNetworkError(const QString& message);
    void onRegisterRequested(const QString& username, const QString& password, const QString& role);
    void onSelfRegisterRequested(const QString& username, const QString& password, const QString& confirmPassword);
    void onQueryRequested(const QString& username);
    void onChangePasswordRequested(const QString& username, const QString& oldPassword, const QString& newPassword);
    void onResetPasswordRequested(const QString& username);
    void onDeleteUserRequested(const QString& username);
    void onSetUserStatusRequested(const QString& username, bool frozen);
    void onUsersQueryRequested(int page, const QString& keyword, const QString& status);
    void onOnlineSessionsRequested();
    void onForceLogoutRequested(const QString& sessionId);
    void onAuditLogsRequested(int page);
    void onOverviewRefreshRequested();
    void onStatsRequested();
    void onBatchSetStatusRequested(const QStringList& usernames, bool frozen);
    void onExportUsersRequested();
    void onMyActivityRequested();
    // 我的空间：登录设备与记录导出。
    void onLogoutDeviceRequested(const QString& sessionId);
    void onExportMyActivityRequested();
    void onLogoutRequested();
    void onRequestTimeouts();
    void onHeartbeat();

private:
    void sendMessage(quint16 messageType, const QByteArray& body, const QString& operation);
    void sendLogin();
    void showLoginPage(const QString& statusMessage = {}, bool error = false);
    void showRegisterPage();
    void showOperationResult(const QString& message, bool error = false);
    void requestUsers(int page);
    void requestAuditLogs(int page, const QString& actor = {});
    void requestOnlineSessions();
    void requestMySessions();
    void sendNextBatchStatus();
    void requestExportPage(int page);
    void finishExport();
    void applyOverviewCounters();
    void centerOnScreen();

    QStackedWidget* stack_{nullptr};
    LoginWidget* login_{nullptr};
    RegisterWidget* register_{nullptr};
    DashboardWidget* dashboard_{nullptr};
    QThread* networkThread_{nullptr};
    QTimer* requestTimer_{nullptr};
    QTimer* heartbeatTimer_{nullptr};
    QtTcpClient* client_{nullptr};
    ClientSession session_;
    QString pendingUsername_;
    QString pendingPassword_;
    // 自助注册时的用户名，注册成功后回填到登录页。
    QString pendingSelfRegisterUsername_;
    QString currentHost_{QStringLiteral("127.0.0.1")};
    quint16 currentPort_{9000};
    // 用户列表的查询状态：关键字 + 状态过滤 + 页码（真实分页）。
    QString usersKeyword_;
    QString usersStatus_;
    // 数据总览的统计值，分多次请求回来后聚合。
    int overviewTotal_{-1};
    int overviewActive_{-1};
    int overviewFrozen_{-1};
    int overviewOnline_{-1};
    // 审计查询去向：audit-page / overview / profile（响应类型相同，按发出时的意图路由）。
    QString auditScope_{QStringLiteral("audit-page")};
    // 批量操作队列：逐条发送，避免一次性打满线程池。
    QStringList batchStatusQueue_;
    bool batchStatusFrozen_{false};
    int batchStatusOk_{0};
    int batchStatusFail_{0};
    // CSV 导出：按页拉全量后写文件。
    QVector<UserRow> exportRows_;
    int exportPageSize_{50};
    QString exportKeyword_;
    QString exportStatus_;
    qint64 lastRttMs_{-1};
    // 当前登录角色（决定强制下线后刷新哪张列表）。
    QString currentRoleCode_{QStringLiteral("user")};
    // 个人中心「我的最近操作」缓存，导出 CSV 时直接复用。
    QVector<AuditLogRow> myActivityRows_;
    bool connected_{false};
    bool waitingLogin_{false};
    bool loggedIn_{false};
};
