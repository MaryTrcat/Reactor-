#pragma once

#include <QStringList>
#include <QVector>
#include <QWidget>

struct UserRow {
    qint64 id{};
    QString username;
    QString role;
    QString status;
};

struct OnlineSessionRow {
    QString sessionId;
    QString username;
    QString role;
    bool authenticated{};
    QString peerAddress;
};

struct AuditLogRow {
    qint64 id{};
    QString time;
    QString actor;
    QString action;
    QString target;
    QString result;
    QString detail;
    QString actorIp;
};

struct ServerStatsRow {
    qint64 uptimeSeconds{};
    int currentConnections{};
    qint64 totalConnections{};
    int ioThreads{};
    int workerThreads{};
    QString storage;
    int activeSessions{};
    qint64 heartbeatRttMs{-1};
};

// WHOAMI 返回的当前账号信息（“我的空间”页展示）。
struct AccountInfoRow {
    qint64 userId{};
    QString username;
    QString role;
    QString status;
    QString peerAddress;
    qint64 authenticatedAt{};
    qint64 lastActiveAt{};
    qint64 serverTimeSeconds{};
};

// 我的登录设备行（个人中心）。
struct MySessionRow {
    QString sessionId;
    QString peerAddress;
    qint64 authenticatedAt{};
    qint64 lastActiveAt{};
    bool current{};
};

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTableWidgetItem;

class DashboardWidget : public QWidget {
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget* parent = nullptr);
    void setUsers(const QVector<UserRow>& users, int total, int page, int pageSize);
    void setOnlineSessions(const QVector<OnlineSessionRow>& sessions);
    void setAuditLogs(const QVector<AuditLogRow>& logs, int total, int page, int pageSize);
    void setCurrentUser(const QString& username, const QString& role);
    // 操作反馈：写入用户管理页操作栏的提示标签（替代原窗口状态栏）。
    void setActionHint(const QString& message, bool error = false);
    // 清掉操作反馈，恢复成中性的选中状态提示（不再显示「失败：…」这类文案）。
    void clearActionHint();

    // 数据总览页。
    void setOverviewStats(int totalUsers, int activeUsers, int frozenUsers, int onlineSessions);
    void setOverviewActivity(const QVector<AuditLogRow>& logs);
    // 服务端监控页。
    void setServerStats(const ServerStatsRow& stats);
    void setHeartbeatRtt(qint64 rttMs);
    // 个人中心页（点击左下角头像进入；普通用户默认页）。
    void setMyActivity(const QVector<AuditLogRow>& logs);
    // 当前账号信息（WHOAMI）。
    void setAccountInfo(const AccountInfoRow& info);
    // 我的登录设备（只含本人会话）。
    void setMySessions(const QVector<MySessionRow>& sessions);
    // 登录后按当前导航页自动拉数据。
    void refreshCurrent();

signals:
    void registerRequested(const QString& username, const QString& password, const QString& role);
    void queryRequested(const QString& username);
    void changePasswordRequested(const QString& username, const QString& oldPassword, const QString& newPassword);
    void resetPasswordRequested(const QString& username);
    void setUserStatusRequested(const QString& username, bool frozen);
    // 删除账号：仅管理员、单条、需二次确认。
    void deleteUserRequested(const QString& username);
    // 用户列表查询：带页码 / 关键字 / 状态过滤（真实分页由服务端执行）。
    void usersQueryRequested(int page, const QString& keyword, const QString& status);
    void refreshRequested();
    void onlineSessionsRequested();
    void forceLogoutRequested(const QString& sessionId);
    void auditLogsRequested(int page);
    // 数据总览：一次拉取统计卡片 + 最近动态。
    void overviewRefreshRequested();
    // 服务端运行指标。
    void statsRequested();
    // 批量操作与导出。
    void batchSetStatusRequested(const QStringList& usernames, bool frozen);
    void exportUsersRequested();
    // 个人中心：按 actor=当前用户 查询最近操作。
    void myActivityRequested();
    // 我的空间：拉取当前账号信息（WHOAMI）。
    void accountInfoRequested();
    // 我的空间：我的登录设备。
    void mySessionsRequested();
    void logoutDeviceRequested(const QString& sessionId);
    // 我的空间：把「我的最近操作」导出成 CSV。
    void exportMyActivityRequested();
    void logoutRequested();

private:
    void switchPage(int row);
    void updateSelectionActions();
    QString selectedUsername() const;
    QStringList selectedUsernames() const;
    void showRegisterDialog();
    void buildOverviewPage();
    void buildProfilePage();
    void openProfilePage();
    QString formatDuration(qint64 seconds) const;
    QString selectedOnlineSessionId() const;
    void applyUsersPagination(int total, int page, int pageSize);
    void applyAuditPagination(int total, int page, int pageSize);

    QListWidget* navigation_{nullptr};
    QStackedWidget* pages_{nullptr};
    // 数据总览页控件。
    QLabel* statTotalLabel_{nullptr};
    QLabel* statActiveLabel_{nullptr};
    QLabel* statFrozenLabel_{nullptr};
    QLabel* statOnlineLabel_{nullptr};
    QLabel* overviewHintLabel_{nullptr};
    QTableWidget* overviewActivityTable_{nullptr};
    // 用户管理页控件。
    QTableWidget* usersTable_{nullptr};
    QLabel* userCountLabel_{nullptr};
    QLabel* userFooterLabel_{nullptr};
    QLabel* userPageLabel_{nullptr};
    QPushButton* previousPageButton_{nullptr};
    QPushButton* nextPageButton_{nullptr};
    QLineEdit* userSearchEdit_{nullptr};
    QComboBox* userStatusFilter_{nullptr};
    QLabel* actionHintLabel_{nullptr};
    QPushButton* resetPasswordButton_{nullptr};
    QPushButton* freezeUserButton_{nullptr};
    QPushButton* batchToggleButton_{nullptr};
    QPushButton* deleteUserButton_{nullptr};
    // 在线会话页控件。
    QTableWidget* onlineTable_{nullptr};
    QPushButton* forceOfflineButton_{nullptr};
    // 审计日志页控件。
    QTableWidget* auditTable_{nullptr};
    QLabel* auditFooterLabel_{nullptr};
    QLabel* auditPageLabel_{nullptr};
    QPushButton* auditPreviousButton_{nullptr};
    QPushButton* auditNextButton_{nullptr};
    // 服务端监控页控件。
    QLabel* statUptimeLabel_{nullptr};
    QLabel* statConnectionsLabel_{nullptr};
    QLabel* statTotalConnectionsLabel_{nullptr};
    QLabel* statThreadsLabel_{nullptr};
    QLabel* statStorageLabel_{nullptr};
    QLabel* statSessionsLabel_{nullptr};
    QLabel* statRttLabel_{nullptr};
    // 个人中心页控件。
    QLabel* profileAvatarLabel_{nullptr};
    QLabel* profileNameLabel_{nullptr};
    QLabel* profileRoleLabel_{nullptr};
    QLabel* profileUserIdLabel_{nullptr};
    QLabel* profileStatusLabel_{nullptr};
    QLabel* profileIpLabel_{nullptr};
    QLabel* profileLoginTimeLabel_{nullptr};
    QLabel* profileActiveLabel_{nullptr};
    QLineEdit* profileOldPasswordEdit_{nullptr};
    QLineEdit* profileNewPasswordEdit_{nullptr};
    QLineEdit* profileConfirmEdit_{nullptr};
    QTableWidget* myActivityTable_{nullptr};
    QTableWidget* mySessionsTable_{nullptr};
    QPushButton* logoutDeviceButton_{nullptr};
    QLabel* profilePasswordChangedLabel_{nullptr};
    QString lastPasswordChangeTime_;
    QString currentUsername_{QStringLiteral("super_admin")};
    QString currentRole_{QStringLiteral("超级管理员")};
    QString currentRoleCode_{QStringLiteral("admin")};
    int usersTotal_{0};
    int usersPage_{1};
    int usersPageSize_{10};
    int auditTotal_{0};
    int auditPage_{1};
    int auditPageSize_{10};
};
