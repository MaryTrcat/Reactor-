#include "MainWindow.h"

#include "DashboardWidget.h"
#include "LoginWidget.h"
#include "RegisterWidget.h"
#include "ProtoMessages.h"
#include "QtTcpClient.h"

#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QGuiApplication>
#include <QMessageBox>
#include <QMetaObject>
#include <QScreen>
#include <QStyle>
#include <QStackedWidget>
#include <QStringConverter>
#include <QTextStream>
#include <QThread>
#include <QTimer>

namespace {

constexpr int kUsersPageSize = 10;
constexpr int kAuditPageSize = 10;
constexpr int kOverviewAuditPageSize = 6;
// 服务端空闲超时为 300 秒，30 秒一次心跳足够保活。
constexpr int kHeartbeatIntervalMs = 30 * 1000;

// 服务端返回的是英文文案，登录失败按关键词映射成中文提示。
QString localizedLoginError(const QString& message) {
    if (message.contains(QStringLiteral("invalid username or password"))) {
        return QStringLiteral("用户名或密码错误");
    }
    if (message.contains(QStringLiteral("too many failed attempts"))) {
        return QStringLiteral("失败次数过多，请 5 分钟后再试");
    }
    if (message.contains(QStringLiteral("frozen"))) {
        return QStringLiteral("账号已被冻结，请联系管理员");
    }
    return message;
}

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Reactor 用户权限工作台"));
    resize(600, 460);
    setMinimumSize(500, 380);
    setStyleSheet(QStringLiteral(
        "QMainWindow { background: #ffffff; }"
        "QWidget { color: #252a30; font-size: 14px; }"
        "QPushButton { min-height: 30px; padding: 0 13px; border: 1px solid #b8b8b8; border-radius: 3px; background: #ffffff; color: #333333; }"
        "QPushButton:hover { background: #eaf3fb; border-color: #7ba9cd; }"
        "QPushButton:pressed { background: #d8e8f8; }"
        "QPushButton:disabled { color: #aaaaaa; background: #f3f3f3; border-color: #d0d0d0; }"
        "QPushButton[primary=\"true\"] { background: #3289ce; border-color: #287ab8; color: #ffffff; font-weight: 600; }"
        "QPushButton[primary=\"true\"]:hover { background: #2678b8; }"
        "QPushButton[danger=\"true\"] { color: #c0392b; border-color: #e0b4ae; }"
        "QPushButton[danger=\"true\"]:hover { background: #fdf0ee; }"
        "QPushButton[danger=\"true\"]:disabled { color: #b9b9b9; border-color: #dcdcdc; }"
        "QLineEdit, QComboBox { min-height: 30px; padding: 0 9px; border: 1px solid #c7c7c7; border-radius: 3px; background: #ffffff; }"
        "QLineEdit:focus, QComboBox:focus { border-color: #3289ce; }"
        "#loginPanel { border: 1px solid #d5d5d5; border-radius: 4px; background: #ffffff; }"
        "#loginPanel #pageTitle { color: #252a30; font-size: 24px; font-weight: 700; }"
        "#loginPanel #pageSubtitle { color: #888888; font-size: 14px; }"
        "#loginPanel QFormLayout QLabel { color: #252a30; font-size: 16px; }"
        "#loginPanel QLineEdit { min-height: 42px; padding: 0 11px; font-size: 16px; }"
        "#loginPanel #loginButton { min-height: 46px; font-size: 17px; }"
        "#registerLink, #backToLoginLink { border: 0; background: transparent; color: #3289ce; font-size: 13px; }"
        "#registerLink:hover, #backToLoginLink:hover { background: transparent; color: #2678b8; text-decoration: underline; }"
        "#registerLink:pressed, #backToLoginLink:pressed { background: transparent; color: #1d6699; }"
        "#statusLabel { color: #888888; font-size: 13px; }"
        "#statusLabel[error=\"true\"] { color: #c03b32; }"
        "#dashboardHeader { background: #ededed; border-bottom: 1px solid #c7c7c7; }"
        "#brandMark { min-width: 18px; max-width: 18px; min-height: 18px; max-height: 18px; border-radius: 3px; background: #287fc0; color: #ffffff; font-size: 11px; font-weight: 700; }"
        "#dashboardTitle { color: #252a30; font-size: 15px; font-weight: 650; }"
        "#dashboardSidebar { background: #ededed; border-right: 1px solid #c5c5c5; }"
        "#navTitle { color: #858585; font-size: 11px; }"
        "#dashboardNavigation { background: transparent; border: 0; padding: 0; }"
        "#dashboardNavigation::item { height: 44px; padding: 0 16px; border-left: 3px solid transparent; color: #343a40; }"
        "#dashboardNavigation::item:hover { background: #e4e8ec; }"
        "#dashboardNavigation::item:selected { background: #d8e8f8; color: #164d78; border-left-color: #2a83c8; font-weight: 650; }"
        "#dashboardNavigation:focus { border: 0; outline: none; }"
        "#accountBar { min-height: 84px; max-height: 84px; border: 0; border-top: 1px solid #d1d1d1; border-radius: 0; background: #ededed; text-align: left; }"
        "#accountBar:hover { background: #e1e5e9; }"
        "#accountAvatar { background: #94a0ad; color: #ffffff; border-radius: 20px; font-weight: 700; font-size: 16px; }"
        "#accountName { color: #252a30; font-weight: 650; font-size: 14px; }"
        "#accountRole, #pageHint, #pageSubtitle { color: #888888; font-size: 11px; }"
        "#pageTitle { color: #252a30; font-size: 16px; font-weight: 700; }"
        "#dashboardPanel { border: 1px solid #c9c9c9; background: #ffffff; }"
        "QTableWidget { background: #ffffff; border: 0; gridline-color: #e7e7e7; selection-background-color: #b8c6d2; selection-color: #202a33; outline: none; }"
        "QHeaderView::section { background: #eaeaea; padding: 8px 12px; border: 0; border-right: 1px solid #d8d8d8; border-bottom: 1px solid #c9c9c9; color: #333333; font-weight: 650; }"
        "QTableWidget::item { padding: 0 12px; border-right: 1px solid #ededed; border-bottom: 1px solid #e7e7e7; }"
        "QTableWidget::item:selected { background: #b8c6d2; color: #202a33; border: 0; }"
        "QTableWidget::item:focus { outline: none; border: 0; }"
        "#tableFooter, #actionBar { background: #fafafa; border-top: 1px solid #dddddd; }"
        "#actionBar { border: 1px solid #dedede; border-radius: 3px; }"
        "#actionLabel, #panelTitle { color: #626262; font-weight: 650; }"
        "#statCard { background: #f5f8fb; border: 1px solid #dde3e9; border-radius: 6px; }"
        "#statValueTotal, #statValueActive, #statValueFrozen, #statValueOnline { color: #2570b8; font-size: 28px; font-weight: 700; }"
        "#statCaption { color: #8a8a8a; font-size: 12px; }"
        "#monitorValue { color: #252a30; font-weight: 600; }"
        "#profileAvatar { background: #3289ce; color: #ffffff; border-radius: 28px; font-size: 22px; font-weight: 700; }"
        "#profileName { color: #252a30; font-size: 18px; font-weight: 700; }"
        "QDialog { background: #ffffff; }"
        "QDialogButtonBox QPushButton[primary=\"true\"] { background: #3289ce; color: #ffffff; }"));

    stack_ = new QStackedWidget(this);
    login_ = new LoginWidget(stack_);
    register_ = new RegisterWidget(stack_);
    dashboard_ = new DashboardWidget(stack_);
    stack_->addWidget(login_);
    stack_->addWidget(register_);
    stack_->addWidget(dashboard_);
    stack_->setCurrentWidget(login_);
    setCentralWidget(stack_);

    // 登录页：窗口尺寸贴合卡片内容，不留空白（额外预留一行状态提示的高度）。
    showLoginPage();

    networkThread_ = new QThread(this);
    client_ = new QtTcpClient();
    client_->moveToThread(networkThread_);
    networkThread_->start();
    requestTimer_ = new QTimer(this);
    requestTimer_->setInterval(1000);
    connect(requestTimer_, &QTimer::timeout, this, &MainWindow::onRequestTimeouts);
    requestTimer_->start();
    heartbeatTimer_ = new QTimer(this);
    heartbeatTimer_->setInterval(kHeartbeatIntervalMs);
    connect(heartbeatTimer_, &QTimer::timeout, this, &MainWindow::onHeartbeat);

    connect(login_, &LoginWidget::loginRequested, this, &MainWindow::onLoginRequested);
    connect(login_, &LoginWidget::registerPageRequested, this, &MainWindow::showRegisterPage);
    connect(register_, &RegisterWidget::registerRequested, this, &MainWindow::onSelfRegisterRequested);
    connect(register_, &RegisterWidget::backRequested, this, [this]() { showLoginPage(); });
    connect(dashboard_, &DashboardWidget::registerRequested, this, &MainWindow::onRegisterRequested);
    connect(dashboard_, &DashboardWidget::queryRequested, this, &MainWindow::onQueryRequested);
    connect(dashboard_, &DashboardWidget::changePasswordRequested, this, &MainWindow::onChangePasswordRequested);
    connect(dashboard_, &DashboardWidget::resetPasswordRequested, this, &MainWindow::onResetPasswordRequested);
    connect(dashboard_, &DashboardWidget::setUserStatusRequested, this, &MainWindow::onSetUserStatusRequested);
    connect(dashboard_, &DashboardWidget::deleteUserRequested, this, &MainWindow::onDeleteUserRequested);
    connect(dashboard_, &DashboardWidget::usersQueryRequested, this, &MainWindow::onUsersQueryRequested);
    connect(dashboard_, &DashboardWidget::onlineSessionsRequested, this, &MainWindow::onOnlineSessionsRequested);
    connect(dashboard_, &DashboardWidget::forceLogoutRequested, this, &MainWindow::onForceLogoutRequested);
    connect(dashboard_, &DashboardWidget::auditLogsRequested, this, &MainWindow::onAuditLogsRequested);
    connect(dashboard_, &DashboardWidget::overviewRefreshRequested, this, &MainWindow::onOverviewRefreshRequested);
    connect(dashboard_, &DashboardWidget::statsRequested, this, &MainWindow::onStatsRequested);
    connect(dashboard_, &DashboardWidget::batchSetStatusRequested, this, &MainWindow::onBatchSetStatusRequested);
    connect(dashboard_, &DashboardWidget::exportUsersRequested, this, &MainWindow::onExportUsersRequested);
    connect(dashboard_, &DashboardWidget::myActivityRequested, this, &MainWindow::onMyActivityRequested);
    connect(dashboard_, &DashboardWidget::accountInfoRequested, this, [this]() {
        if (loggedIn_) sendMessage(29, qtproto::whoamiRequest(), QStringLiteral("whoami"));
    });
    connect(dashboard_, &DashboardWidget::logoutRequested, this, &MainWindow::onLogoutRequested);
    connect(dashboard_, &DashboardWidget::mySessionsRequested, this, [this]() { requestMySessions(); });
    connect(dashboard_, &DashboardWidget::logoutDeviceRequested, this, &MainWindow::onLogoutDeviceRequested);
    connect(dashboard_, &DashboardWidget::exportMyActivityRequested, this, &MainWindow::onExportMyActivityRequested);
    connect(client_, &QtTcpClient::connected, this, &MainWindow::onConnected, Qt::QueuedConnection);
    connect(client_, &QtTcpClient::disconnected, this, &MainWindow::onDisconnected, Qt::QueuedConnection);
    connect(client_, &QtTcpClient::frameReceived, this, &MainWindow::onFrameReceived, Qt::QueuedConnection);
    connect(client_, &QtTcpClient::errorOccurred, this, &MainWindow::onNetworkError, Qt::QueuedConnection);

    QTimer::singleShot(0, this, [this]() {
        centerOnScreen();
        QMetaObject::invokeMethod(client_, [this]() {
            client_->connectToServer(currentHost_, currentPort_);
        }, Qt::QueuedConnection);
    });
}

MainWindow::~MainWindow() {
    if (client_) {
        QMetaObject::invokeMethod(client_, "disconnectFromServer", Qt::BlockingQueuedConnection);
    }
    if (networkThread_) {
        networkThread_->quit();
        networkThread_->wait();
    }
    delete client_;
}

void MainWindow::onLoginRequested(const QString& username, const QString& password) {
    pendingUsername_ = username;
    pendingPassword_ = password;
    waitingLogin_ = true;
    // 连接过程的提示不再展示（状态标签只用于登录失败等结果反馈）。
    if (connected_) {
        sendLogin();
        return;
    }
    QMetaObject::invokeMethod(client_, [this]() { client_->connectToServer(currentHost_, currentPort_); }, Qt::QueuedConnection);
}

void MainWindow::autoLogin(const QString& username, const QString& password) {
    onLoginRequested(username, password);
}

void MainWindow::onConnected() {
    connected_ = true;
    if (waitingLogin_) sendLogin();
}

void MainWindow::onDisconnected() {
    connected_ = false;
    loggedIn_ = false;
    if (heartbeatTimer_) heartbeatTimer_->stop();
    showOperationResult(QStringLiteral("服务端连接已断开"), true);
}

void MainWindow::onFrameReceived(quint16 messageType, quint32 requestId, const QByteArray& body) {
    const QString operation = session_.take(requestId);
    if (operation.isEmpty()) {
        // 心跳响应没有注册过操作名，静默处理。
        if (messageType == 24) return;
        return;
    }

    if (messageType == 100) {
        QString message;
        if (qtproto::decodeErrorResponse(body, message)) {
            showOperationResult(message, true);
        }
        return;
    }

    if (messageType == 24) {
        qtproto::PongInfo pong;
        if (qtproto::decodePongResponse(body, pong)) {
            const auto rtt = pong.serverTimeMs - pong.clientTimeMs;
            lastRttMs_ = rtt;
            dashboard_->setHeartbeatRtt(rtt);
        }
        return;
    }

    if (messageType == 4) {
        qtproto::LoginResponse response;
        if (!qtproto::decodeLoginResponse(body, response)) {
            login_->setStatus(QStringLiteral("登录响应无效"), true);
        } else if (response.success) {
            waitingLogin_ = false;
            loggedIn_ = true;
            currentRoleCode_ = response.role;
            myActivityRows_.clear();
            resize(1180, 760);
            setMinimumSize(1000, 680);
            centerOnScreen();
            dashboard_->setCurrentUser(pendingUsername_, response.role);
            stack_->setCurrentWidget(dashboard_);
            showOperationResult(QStringLiteral("登录成功：%1 (%2)").arg(pendingUsername_, response.role));
            if (heartbeatTimer_ && !heartbeatTimer_->isActive()) heartbeatTimer_->start();
            if (response.role == QStringLiteral("admin")) {
                // 登录后按当前导航页（默认数据总览）自动拉数据。
                dashboard_->refreshCurrent();
            } else {
                // 普通用户默认落在「我的空间」；setCurrentUser 切导航会触发 whoami + 我的操作拉取。
            }
        } else {
            // 登录失败：映射成中文提示（不再显示剩余尝试次数）。
            login_->setStatus(localizedLoginError(response.message), true);
        }
        return;
    }

    if (messageType == 14) {
        qtproto::ListUsersResponse response;
        if (qtproto::decodeListUsersResponse(body, response)) {
            QVector<UserRow> users;
            users.reserve(response.users.size());
            for (const auto& item : response.users) {
                users.push_back({item.userId, item.username, item.role, item.status});
            }
            if (operation == QStringLiteral("overview-total")) {
                overviewTotal_ = response.total;
                applyOverviewCounters();
            } else if (operation == QStringLiteral("overview-active") || operation == QStringLiteral("overview-frozen")) {
                if (operation == QStringLiteral("overview-active")) overviewActive_ = response.total;
                else overviewFrozen_ = response.total;
                applyOverviewCounters();
            } else if (operation == QStringLiteral("export-users")) {
                exportRows_ += users;
                const int fetched = exportRows_.size();
                if (fetched < response.total && response.users.size() > 0) {
                    requestExportPage(response.page + 1);
                } else {
                    finishExport();
                }
            } else {
                dashboard_->setUsers(users, response.total, response.page, response.pageSize);
                showOperationResult(QStringLiteral("已加载第 %1 页，共 %2 个用户").arg(response.page).arg(response.total));
            }
        }
        return;
    }

    if (messageType == 26) {
        qtproto::ListAuditLogsResponse response;
        if (qtproto::decodeListAuditLogsResponse(body, response)) {
            QVector<AuditLogRow> rows;
            rows.reserve(response.entries.size());
            for (const auto& item : response.entries) {
                const auto time = QDateTime::fromSecsSinceEpoch(item.createdAt).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
                rows.push_back({item.id, time, item.actor, item.action, item.target, item.result, item.detail, item.actorIp});
            }
            if (operation == QStringLiteral("overview-audit")) {
                dashboard_->setOverviewActivity(rows);
            } else if (operation == QStringLiteral("profile-audit")) {
                myActivityRows_ = rows;
                dashboard_->setMyActivity(rows);
            } else {
                dashboard_->setAuditLogs(rows, response.total, response.page, response.pageSize);
                showOperationResult(QStringLiteral("已加载第 %1 页，共 %2 条日志").arg(response.page).arg(response.total));
            }
        }
        return;
    }

    if (messageType == 28) {
        qtproto::StatsInfo stats;
        if (qtproto::decodeStatsResponse(body, stats)) {
            if (!stats.success) {
                showOperationResult(stats.message.isEmpty() ? QStringLiteral("无权查看服务端指标") : stats.message, true);
                return;
            }
            ServerStatsRow row;
            row.uptimeSeconds = stats.uptimeSeconds;
            row.currentConnections = stats.currentConnections;
            row.totalConnections = stats.totalConnections;
            row.ioThreads = stats.ioThreads;
            row.workerThreads = stats.workerThreads;
            row.storage = stats.storage;
            row.activeSessions = stats.activeSessions;
            row.heartbeatRttMs = lastRttMs_;
            dashboard_->setServerStats(row);
        }
        return;
    }

    if (messageType == 30) {
        qtproto::AccountInfo info;
        if (qtproto::decodeWhoamiResponse(body, info) && info.success) {
            AccountInfoRow row;
            row.userId = info.userId;
            row.username = info.username;
            row.role = info.role;
            row.status = info.status;
            row.peerAddress = info.peerAddress;
            row.authenticatedAt = info.authenticatedAt;
            row.lastActiveAt = info.lastActiveAt;
            row.serverTimeSeconds = info.serverTimeSeconds;
            dashboard_->setAccountInfo(row);
        }
        return;
    }

    if (messageType == 6) {
        qtproto::QueryUserResponse response;
        if (qtproto::decodeQueryUserResponse(body, response)) {
            showOperationResult(response.message, !response.found);
        }
        return;
    }

    if (messageType == 2) {
        qtproto::RegisterResponse response;
        if (qtproto::decodeRegisterResponse(body, response)) {
            if (operation == QStringLiteral("self-register")) {
                // 自助注册：成功回登录页并预填用户名；失败留在注册页提示。
                if (response.success) {
                    login_->setUsername(pendingSelfRegisterUsername_);
                    showLoginPage(QStringLiteral("注册成功，请登录"));
                } else {
                    register_->setStatus(response.message, true);
                }
            } else {
                showOperationResult(response.message, !response.success);
                if (response.success) requestUsers(1);
            }
        }
        return;
    }

    if (messageType == 10) {
        qtproto::OperationResponse response;
        if (qtproto::decodeOperationResponse(body, response)) {
            showOperationResult(response.message, !response.success);
        }
        return;
    }

    if (messageType == 12) {
        qtproto::OperationResponse response;
        if (qtproto::decodeOperationResponse(body, response)) {
            showOperationResult(response.message, !response.success);
            if (response.success) requestUsers(1);
        }
        return;
    }

    if (messageType == 16) {
        // 重置密码：一次性随机口令必须当场弹窗展示，之后服务端不再返回。
        qtproto::ResetPasswordResponse response;
        if (qtproto::decodeResetPasswordResponse(body, response)) {
            if (response.success && !response.generatedPassword.isEmpty()) {
                QMessageBox::information(this, QStringLiteral("重置密码成功"),
                                         QStringLiteral("新密码（仅显示这一次，请立即转达用户并妥善保存）：\n\n%1")
                                             .arg(response.generatedPassword));
            } else {
                showOperationResult(response.message, !response.success);
            }
            if (response.success) requestUsers(1);
        }
        return;
    }

    if (messageType == 18) {
        qtproto::OperationResponse response;
        if (qtproto::decodeOperationResponse(body, response)) {
            showOperationResult(response.message, !response.success);
            if (response.success) requestUsers(1);
        }
        return;
    }

    if (messageType == 20) {
        qtproto::ListOnlineResponse response;
        if (qtproto::decodeListOnlineResponse(body, response)) {
            if (operation == QStringLiteral("overview-online")) {
                overviewOnline_ = response.sessions.size();
                applyOverviewCounters();
                return;
            }
            QVector<OnlineSessionRow> sessions;
            sessions.reserve(response.sessions.size());
            for (const auto& item : response.sessions) {
                sessions.push_back({item.sessionId, item.username, item.role, item.authenticated, item.peerAddress});
            }
            dashboard_->setOnlineSessions(sessions);
            showOperationResult(response.message, !response.success);
        }
        return;
    }

    if (messageType == 18) {
        qtproto::OperationResponse response;
        if (qtproto::decodeOperationResponse(body, response)) {
            if (operation == QStringLiteral("batch-status")) {
                if (response.success) ++batchStatusOk_;
                else {
                    ++batchStatusFail_;
                    showOperationResult(response.message, true);
                }
                sendNextBatchStatus();
                return;
            }
            showOperationResult(response.message, !response.success);
            if (response.success) requestUsers(1);
        }
        return;
    }

    if (messageType == 12) {
        qtproto::OperationResponse response;
        if (qtproto::decodeOperationResponse(body, response)) {
            showOperationResult(response.message, !response.success);
            if (response.success) requestUsers(1);
        }
        return;
    }

    if (messageType == 32) {
        // 我的登录设备：只含本人会话，服务端已按会话身份过滤。
        qtproto::MySessionsResponse response;
        if (qtproto::decodeMySessionsResponse(body, response)) {
            if (response.success) {
                QVector<MySessionRow> rows;
                rows.reserve(response.sessions.size());
                for (const auto& item : response.sessions) {
                    rows.push_back({item.sessionId, item.peerAddress, item.authenticatedAt, item.lastActiveAt, item.current});
                }
                dashboard_->setMySessions(rows);
            }
        }
        return;
    }

    if (messageType == 22) {
        qtproto::OperationResponse response;
        if (qtproto::decodeOperationResponse(body, response)) {
            showOperationResult(response.message, !response.success);
            if (response.success) {
                // 管理员踢人后刷新在线列表；普通用户踢自己的旧设备后刷新设备列表。
                if (currentRoleCode_ == QStringLiteral("admin")) requestOnlineSessions();
                else requestMySessions();
                dashboard_->myActivityRequested();
            }
        }
        return;
    }
}

void MainWindow::onNetworkError(const QString& message) {
    // 登录/注册页上的网络错误必须直接展示（此前送到用户页提示标签导致失败无任何反馈）。
    if (!loggedIn_ && stack_->currentWidget() == login_) {
        waitingLogin_ = false;
        login_->setStatus(message, true);
        return;
    }
    if (!loggedIn_ && stack_->currentWidget() == register_) {
        register_->setStatus(message, true);
        return;
    }
    showOperationResult(QStringLiteral("网络错误：") + message, true);
}

void MainWindow::onRegisterRequested(const QString& username, const QString& password, const QString& role) {
    const auto normalizedUsername = username.trimmed();
    if (normalizedUsername.isEmpty()) {
        showOperationResult(QStringLiteral("用户名不能为空"), true);
        return;
    }
    if (password.size() < 6) {
        showOperationResult(QStringLiteral("新增失败：密码至少需要 6 位（当前 %1 位）").arg(password.size()), true);
        return;
    }
    sendMessage(1, qtproto::registerRequest(normalizedUsername, password, role), QStringLiteral("register"));
}

void MainWindow::onQueryRequested(const QString& username) {
    sendMessage(5, qtproto::queryUserRequest(username), QStringLiteral("query"));
}

void MainWindow::onChangePasswordRequested(const QString& username, const QString& oldPassword, const QString& newPassword) {
    const auto normalizedUsername = username.trimmed();
    if (normalizedUsername.isEmpty()) {
        showOperationResult(QStringLiteral("用户名不能为空"), true);
        return;
    }
    if (oldPassword.isEmpty()) {
        showOperationResult(QStringLiteral("请输入当前密码后再修改"), true);
        return;
    }
    if (newPassword.size() < 6) {
        showOperationResult(QStringLiteral("新密码至少需要 6 位"), true);
        return;
    }
    if (oldPassword == newPassword) {
        showOperationResult(QStringLiteral("新密码不能与旧密码相同"), true);
        return;
    }
    sendMessage(9, qtproto::changePasswordRequest(normalizedUsername, oldPassword, newPassword), QStringLiteral("change-password"));
}

void MainWindow::onResetPasswordRequested(const QString& username) {
    sendMessage(15, qtproto::resetPasswordRequest(username), QStringLiteral("reset-password"));
}

void MainWindow::onSetUserStatusRequested(const QString& username, bool frozen) {
    sendMessage(17, qtproto::setUserStatusRequest(username, frozen), frozen ? QStringLiteral("freeze-user") : QStringLiteral("unfreeze-user"));
}

void MainWindow::onDeleteUserRequested(const QString& username) {
    if (username.isEmpty()) return;
    sendMessage(11, qtproto::deleteUserRequest(username), QStringLiteral("delete-user"));
}

void MainWindow::onLogoutDeviceRequested(const QString& sessionId) {
    if (sessionId.isEmpty()) return;
    sendMessage(21, qtproto::forceLogoutRequest(sessionId), QStringLiteral("logout-device"));
}

void MainWindow::requestMySessions() {
    if (!loggedIn_) return;
    sendMessage(31, qtproto::mySessionsRequest(), QStringLiteral("my-sessions"));
}

void MainWindow::onExportMyActivityRequested() {
    if (myActivityRows_.isEmpty()) {
        dashboard_->setActionHint(QStringLiteral("暂无可导出的记录，先点「刷新」拉取一次"));
        return;
    }
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出我的操作记录"),
                                                      QStringLiteral("my-activity.csv"), QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("导出失败"), QStringLiteral("无法写入文件：%1").arg(path));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);
    out << QStringLiteral("时间,操作,对象,结果\n");
    // 简单 CSV 转义：字段里的逗号/引号用双引号包裹。
    const auto escape = [](QString value) {
        if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"'))) {
            value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
            return QLatin1Char('"') + value + QLatin1Char('"');
        }
        return value;
    };
    for (const auto& row : myActivityRows_) {
        out << escape(row.time) << QLatin1Char(',') << escape(row.action) << QLatin1Char(',')
            << escape(row.target) << QLatin1Char(',') << escape(row.result) << QLatin1Char('\n');
    }
    QMessageBox::information(this, QStringLiteral("导出完成"),
                             QStringLiteral("已导出 %1 条记录到\n%2").arg(myActivityRows_.size()).arg(path));
}

void MainWindow::onUsersQueryRequested(int page, const QString& keyword, const QString& status) {
    usersKeyword_ = keyword;
    usersStatus_ = status;
    requestUsers(page);
}

void MainWindow::onOnlineSessionsRequested() {
    requestOnlineSessions();
}

void MainWindow::onForceLogoutRequested(const QString& sessionId) {
    sendMessage(21, qtproto::forceLogoutRequest(sessionId), QStringLiteral("force-logout"));
}

void MainWindow::onAuditLogsRequested(int page) {
    requestAuditLogs(page);
}

void MainWindow::onLogoutRequested() {
    waitingLogin_ = false;
    loggedIn_ = false;
    pendingUsername_.clear();
    pendingPassword_.clear();
    usersKeyword_.clear();
    usersStatus_.clear();
    batchStatusQueue_.clear();
    exportRows_.clear();
    lastRttMs_ = -1;
    myActivityRows_.clear();
    currentRoleCode_ = QStringLiteral("user");
    session_ = ClientSession{};
    if (heartbeatTimer_) heartbeatTimer_->stop();
    connected_ = false;
    pendingSelfRegisterUsername_.clear();
    showLoginPage(QStringLiteral("已退出账号"));
    QMetaObject::invokeMethod(client_, "disconnectFromServer", Qt::QueuedConnection);
}

void MainWindow::onRequestTimeouts() {
    const auto expired = session_.expire(QDateTime::currentMSecsSinceEpoch(), 5000);
    for (const auto& item : expired) {
        showOperationResult(QStringLiteral("请求超时：%1 request_id=%2").arg(item.second).arg(item.first), true);
        // 批量/导出请求超时也要推进队列，否则后续项会被卡死。
        if (item.second == QStringLiteral("batch-status")) sendNextBatchStatus();
        else if (item.second == QStringLiteral("export-users")) finishExport();
        else if (item.second == QStringLiteral("self-register")) register_->setStatus(QStringLiteral("注册请求超时，请重试"), true);
    }
}

void MainWindow::onHeartbeat() {
    if (!connected_ || !loggedIn_) return;
    sendMessage(23, qtproto::pingRequest(QDateTime::currentMSecsSinceEpoch()), QStringLiteral("heartbeat"));
}

void MainWindow::sendMessage(quint16 messageType, const QByteArray& body, const QString& operation) {
    if (!connected_) {
        showOperationResult(QStringLiteral("服务端未连接"), true);
        return;
    }
    const quint32 requestId = session_.begin(operation);
    QMetaObject::invokeMethod(client_, [this, messageType, requestId, body]() {
        client_->sendFrame(messageType, requestId, body);
    }, Qt::QueuedConnection);
}

void MainWindow::sendLogin() {
    sendMessage(3, qtproto::loginRequest(pendingUsername_, pendingPassword_), QStringLiteral("login"));
}

void MainWindow::showLoginPage(const QString& statusMessage, bool error) {
    stack_->setCurrentWidget(login_);
    if (!statusMessage.isEmpty()) {
        login_->setStatus(statusMessage, error);
    }
    // 先放宽最小尺寸再 resize：顺序反了会被旧的最小尺寸卡住，窗口缩不下去。
    // 登录页与注册页尺寸不同，切换时按当前页面重新贴合窗口。
    const QSize page = stack_->currentWidget()->sizeHint() + QSize(0, 24);
    setMinimumSize(page);
    resize(page);
    centerOnScreen();
}

void MainWindow::showRegisterPage() {
    register_->setStatus(QString{});
    stack_->setCurrentWidget(register_);
    const QSize page = register_->sizeHint() + QSize(0, 24);
    setMinimumSize(page);
    resize(page);
    centerOnScreen();
}

void MainWindow::onSelfRegisterRequested(const QString& username, const QString& password, const QString& confirmPassword) {
    if (password != confirmPassword) {
        register_->setStatus(QStringLiteral("两次输入的密码不一致"), true);
        return;
    }
    pendingSelfRegisterUsername_ = username;
    sendMessage(1, qtproto::registerRequest(username, password, QStringLiteral("user")), QStringLiteral("self-register"));
}

void MainWindow::showOperationResult(const QString& message, bool error) {
    // 失败结果不再在操作栏里显示「失败：…」这类提示（用户反馈这类红字干扰阅读），
    // 失败时只把提示恢复成中性的选中状态文案；成功结果照常提示。
    if (!dashboard_) return;
    if (error) {
        dashboard_->clearActionHint();
        return;
    }
    dashboard_->setActionHint(QStringLiteral("完成：") + message, false);
}

void MainWindow::requestUsers(int page) {
    sendMessage(13, qtproto::listUsersRequest(page, kUsersPageSize, usersKeyword_, usersStatus_), QStringLiteral("list-users"));
}

void MainWindow::requestAuditLogs(int page, const QString& actor) {
    auditScope_ = actor.isEmpty() ? QStringLiteral("audit-page") : QStringLiteral("profile-audit");
    sendMessage(25, qtproto::listAuditLogsRequest(page, kAuditPageSize, actor), auditScope_);
}

void MainWindow::onOverviewRefreshRequested() {
    overviewTotal_ = -1;
    overviewActive_ = -1;
    overviewFrozen_ = -1;
    overviewOnline_ = -1;
    dashboard_->setOverviewStats(0, 0, 0, 0);
    // 五个请求并行发出：总数 / 正常 / 冻结 / 在线 / 最近动态，响应按操作名回填。
    sendMessage(13, qtproto::listUsersRequest(1, 1, QString{}, QString{}), QStringLiteral("overview-total"));
    sendMessage(13, qtproto::listUsersRequest(1, 1, QString{}, QStringLiteral("active")), QStringLiteral("overview-active"));
    sendMessage(13, qtproto::listUsersRequest(1, 1, QString{}, QStringLiteral("frozen")), QStringLiteral("overview-frozen"));
    sendMessage(19, qtproto::listOnlineRequest(), QStringLiteral("overview-online"));
    sendMessage(25, qtproto::listAuditLogsRequest(1, kOverviewAuditPageSize, QString{}), QStringLiteral("overview-audit"));
}

void MainWindow::applyOverviewCounters() {
    dashboard_->setOverviewStats(qMax(0, overviewTotal_), qMax(0, overviewActive_), qMax(0, overviewFrozen_),
                                 qMax(0, overviewOnline_));
}

void MainWindow::onStatsRequested() {
    sendMessage(27, qtproto::statsRequest(), QStringLiteral("server-stats"));
}

void MainWindow::onBatchSetStatusRequested(const QStringList& usernames, bool frozen) {
    if (usernames.isEmpty()) return;
    batchStatusQueue_ = usernames;
    batchStatusFrozen_ = frozen;
    batchStatusOk_ = 0;
    batchStatusFail_ = 0;
    sendNextBatchStatus();
}

void MainWindow::sendNextBatchStatus() {
    if (batchStatusQueue_.isEmpty()) {
        showOperationResult(QStringLiteral("批量%1完成：成功 %2 个，失败 %3 个")
                                .arg(batchStatusFrozen_ ? QStringLiteral("冻结") : QStringLiteral("解冻"))
                                .arg(batchStatusOk_)
                                .arg(batchStatusFail_));
        requestUsers(1);
        return;
    }
    const auto username = batchStatusQueue_.takeFirst();
    sendMessage(17, qtproto::setUserStatusRequest(username, batchStatusFrozen_), QStringLiteral("batch-status"));
}

void MainWindow::onExportUsersRequested() {
    exportRows_.clear();
    exportKeyword_ = usersKeyword_;
    exportStatus_ = usersStatus_;
    requestExportPage(1);
}

void MainWindow::requestExportPage(int page) {
    sendMessage(13, qtproto::listUsersRequest(page, exportPageSize_, exportKeyword_, exportStatus_),
                QStringLiteral("export-users"));
}

void MainWindow::finishExport() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出用户 CSV"),
                                                      QStringLiteral("users.csv"), QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty()) {
        showOperationResult(QStringLiteral("已取消导出"));
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showOperationResult(QStringLiteral("无法写入文件：%1").arg(path), true);
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);  // 带 BOM，Excel 打开中文不乱码
    out << QStringLiteral("ID,用户名,角色,状态\n");
    for (const auto& user : exportRows_) {
        out << user.id << QLatin1Char(',') << user.username << QLatin1Char(',') << user.role << QLatin1Char(',')
            << (user.status == QStringLiteral("frozen") ? QStringLiteral("已冻结") : QStringLiteral("正常")) << QLatin1Char('\n');
    }
    showOperationResult(QStringLiteral("已导出 %1 个账号到 %2").arg(exportRows_.size()).arg(path));
}

void MainWindow::onMyActivityRequested() {
    if (!loggedIn_) return;
    requestAuditLogs(1, pendingUsername_);
}

void MainWindow::requestOnlineSessions() {
    sendMessage(19, qtproto::listOnlineRequest(), QStringLiteral("list-online"));
}

void MainWindow::centerOnScreen() {
    auto* targetScreen = screen();
    if (!targetScreen) {
        targetScreen = QGuiApplication::primaryScreen();
    }
    if (!targetScreen) return;
    const auto available = targetScreen->availableGeometry();
    // 屏幕可视区域放不下窗口时先收缩，避免底部（左下角账号名片）被任务栏或屏幕边缘裁掉。
    const QSize maxSize(available.width() - 16, available.height() - 16);
    QSize fitted = size().boundedTo(maxSize);
    if (fitted != size()) {
        // 同步放宽最小尺寸限制，否则 resize 会被 minimumSize 顶回去。
        setMinimumSize(qMin(minimumSize().width(), fitted.width()), qMin(minimumSize().height(), fitted.height()));
        resize(fitted);
    }
    move(available.center() - rect().center());
}
