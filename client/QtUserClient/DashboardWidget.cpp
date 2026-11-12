#include "DashboardWidget.h"

#include <QAbstractItemView>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace {

QPushButton* button(const QString& text, QWidget* parent, const QString& name = {}) {
    auto* result = new QPushButton(text, parent);
    if (!name.isEmpty()) result->setObjectName(name);
    return result;
}

QLabel* title(const QString& text, QWidget* parent) {
    auto* result = new QLabel(text, parent);
    result->setObjectName(QStringLiteral("pageTitle"));
    return result;
}

QFrame* panel(QWidget* parent) {
    auto* result = new QFrame(parent);
    result->setObjectName(QStringLiteral("dashboardPanel"));
    return result;
}

int pageCount(int total, int pageSize) {
    if (pageSize <= 0) return 1;
    return qMax(1, (total + pageSize - 1) / pageSize);
}

}  // namespace

DashboardWidget::DashboardWidget(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QFrame(this);
    header->setObjectName(QStringLiteral("dashboardHeader"));
    header->setFixedHeight(46);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(18, 0, 18, 0);
    headerLayout->setSpacing(10);
    auto* mark = new QLabel(QStringLiteral("R"), header);
    mark->setObjectName(QStringLiteral("brandMark"));
    mark->setAlignment(Qt::AlignCenter);
    auto* brand = new QLabel(QStringLiteral("工作台"), header);
    brand->setObjectName(QStringLiteral("dashboardTitle"));
    headerLayout->addWidget(mark);
    headerLayout->addWidget(brand);
    headerLayout->addStretch();
    root->addWidget(header);

    auto* body = new QWidget(this);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    auto* sidebar = new QFrame(body);
    sidebar->setObjectName(QStringLiteral("dashboardSidebar"));
    sidebar->setFixedWidth(232);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);
    auto* navTitle = new QLabel(QStringLiteral("工作区"), sidebar);
    navTitle->setObjectName(QStringLiteral("navTitle"));
    navTitle->setContentsMargins(18, 17, 18, 8);
    sidebarLayout->addWidget(navTitle);
    navigation_ = new QListWidget(sidebar);
    navigation_->setObjectName(QStringLiteral("dashboardNavigation"));
    navigation_->addItems({QStringLiteral("数据总览"), QStringLiteral("用户管理"), QStringLiteral("在线用户管理"),
                           QStringLiteral("审计日志"), QStringLiteral("服务端监控"), QStringLiteral("我的空间")});
    navigation_->setFocusPolicy(Qt::NoFocus);
    navigation_->setSelectionMode(QAbstractItemView::SingleSelection);
    navigation_->setCurrentRow(0);
    sidebarLayout->addWidget(navigation_, 1);

    auto* account = new QPushButton(sidebar);
    account->setObjectName(QStringLiteral("accountBar"));
    account->setFlat(true);
    account->setCursor(Qt::PointingHandCursor);
    account->setToolTip(QStringLiteral("打开当前账号设置"));
    account->setFixedHeight(84);
    auto* accountLayout = new QHBoxLayout(account);
    accountLayout->setContentsMargins(16, 12, 12, 12);
    accountLayout->setSpacing(12);
    auto* avatar = new QLabel(QStringLiteral("S"), account);
    avatar->setObjectName(QStringLiteral("accountAvatar"));
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setFixedSize(40, 40);
    auto* accountText = new QVBoxLayout();
    accountText->setSpacing(4);
    auto* accountName = new QLabel(QStringLiteral("super_admin"), account);
    accountName->setObjectName(QStringLiteral("accountName"));
    accountName->setMinimumHeight(24);
    accountName->setMaximumHeight(28);
    accountName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    accountName->setTextInteractionFlags(Qt::NoTextInteraction);
    accountName->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* accountRole = new QLabel(QStringLiteral("超级管理员"), account);
    accountRole->setObjectName(QStringLiteral("accountRole"));
    accountRole->setMinimumHeight(18);
    accountRole->setMaximumHeight(20);
    accountRole->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    accountRole->setAttribute(Qt::WA_TransparentForMouseEvents);
    accountText->addWidget(accountName);
    accountText->addWidget(accountRole);
    accountLayout->addWidget(avatar, 0, Qt::AlignVCenter);
    accountLayout->addLayout(accountText, 1);
    accountLayout->setAlignment(accountText, Qt::AlignVCenter);
    accountLayout->addStretch();
    auto* accountArrow = new QLabel(QStringLiteral("›"), account);
    accountArrow->setAttribute(Qt::WA_TransparentForMouseEvents);
    accountLayout->addWidget(accountArrow);
    sidebarLayout->addWidget(account);
    bodyLayout->addWidget(sidebar);

    pages_ = new QStackedWidget(body);
    pages_->setObjectName(QStringLiteral("dashboardPages"));
    bodyLayout->addWidget(pages_, 1);
    root->addWidget(body, 1);

    // 页面顺序必须与导航一一对应：0 总览 / 1 用户 / 2 在线 / 3 审计 / 4 监控；5 个人中心（头像进入）。
    buildOverviewPage();

    // ===== 用户管理页：搜索 + 状态过滤 + 服务端真实分页 =====
    auto* usersPage = new QWidget(pages_);
    auto* usersLayout = new QVBoxLayout(usersPage);
    usersLayout->setContentsMargins(24, 20, 24, 18);
    usersLayout->setSpacing(14);
    auto* usersHead = new QHBoxLayout();
    usersHead->addWidget(title(QStringLiteral("用户管理"), usersPage));
    userCountLabel_ = new QLabel(QStringLiteral("共 0 个账号"), usersPage);
    userCountLabel_->setObjectName(QStringLiteral("pageSubtitle"));
    usersHead->addWidget(userCountLabel_);
    usersHead->addStretch();

    auto* searchBox = new QWidget(usersPage);
    auto* searchLayout = new QHBoxLayout(searchBox);
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(8);
    userSearchEdit_ = new QLineEdit(searchBox);
    userSearchEdit_->setObjectName(QStringLiteral("userSearchEdit"));
    userSearchEdit_->setPlaceholderText(QStringLiteral("按用户名搜索"));
    userSearchEdit_->setClearButtonEnabled(true);
    userSearchEdit_->setFixedWidth(200);
    searchLayout->addWidget(userSearchEdit_);
    userStatusFilter_ = new QComboBox(searchBox);
    userStatusFilter_->setObjectName(QStringLiteral("userStatusFilter"));
    userStatusFilter_->addItems({QStringLiteral("全部状态"), QStringLiteral("正常"), QStringLiteral("已冻结")});
    searchLayout->addWidget(userStatusFilter_);
    auto* searchButton = button(QStringLiteral("搜索"), searchBox, QStringLiteral("searchUsersButton"));
    searchButton->setProperty("primary", true);
    searchLayout->addWidget(searchButton);
    usersHead->addWidget(searchBox);
    usersHead->addWidget(button(QStringLiteral("刷新列表"), usersPage, QStringLiteral("refreshUsersButton")));
    usersLayout->addLayout(usersHead);

    auto* userPanel = panel(usersPage);
    auto* userPanelLayout = new QVBoxLayout(userPanel);
    userPanelLayout->setContentsMargins(0, 0, 0, 0);
    userPanelLayout->setSpacing(0);
    usersTable_ = new QTableWidget(userPanel);
    usersTable_->setObjectName(QStringLiteral("usersTable"));
    usersTable_->setColumnCount(4);
    usersTable_->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("用户名"), QStringLiteral("角色"), QStringLiteral("账号状态")});
    usersTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    usersTable_->horizontalHeader()->resizeSection(0, 70);
    usersTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    usersTable_->horizontalHeader()->resizeSection(1, 190);
    usersTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    usersTable_->horizontalHeader()->resizeSection(2, 130);
    usersTable_->horizontalHeader()->setStretchLastSection(true);
    usersTable_->verticalHeader()->setVisible(false);
    usersTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    // 批量操作需要多选（Ctrl/Shift）。
    usersTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    usersTable_->setFocusPolicy(Qt::NoFocus);
    usersTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    userPanelLayout->addWidget(usersTable_, 1);
    auto* footer = new QFrame(userPanel);
    footer->setObjectName(QStringLiteral("tableFooter"));
    footer->setFixedHeight(38);
    auto* footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(12, 0, 12, 0);
    userFooterLabel_ = new QLabel(QStringLiteral("共 0 条记录"), footer);
    userFooterLabel_->setObjectName(QStringLiteral("pageHint"));
    footerLayout->addWidget(userFooterLabel_);
    footerLayout->addStretch();
    previousPageButton_ = button(QStringLiteral("上一页"), footer, QStringLiteral("previousPageButton"));
    footerLayout->addWidget(previousPageButton_);
    userPageLabel_ = new QLabel(QStringLiteral("第 1 / 1 页"), footer);
    userPageLabel_->setObjectName(QStringLiteral("userPageLabel"));
    footerLayout->addWidget(userPageLabel_);
    nextPageButton_ = button(QStringLiteral("下一页"), footer, QStringLiteral("nextPageButton"));
    footerLayout->addWidget(nextPageButton_);
    userPanelLayout->addWidget(footer);
    usersLayout->addWidget(userPanel, 1);

    auto* actions = new QFrame(usersPage);
    actions->setObjectName(QStringLiteral("actionBar"));
    actions->setFixedHeight(58);
    auto* actionsLayout = new QHBoxLayout(actions);
    actionsLayout->setContentsMargins(14, 0, 14, 0);
    actionsLayout->setSpacing(9);
    auto* actionLabel = new QLabel(QStringLiteral("选中用户操作"), actions);
    actionLabel->setObjectName(QStringLiteral("actionLabel"));
    actionsLayout->addWidget(actionLabel);
    auto* newUserButton = button(QStringLiteral("新增用户"), actions, QStringLiteral("newUserButton"));
    newUserButton->setProperty("primary", true);
    actionsLayout->addWidget(newUserButton);
    resetPasswordButton_ = button(QStringLiteral("重置密码"), actions, QStringLiteral("resetPasswordButton"));
    actionsLayout->addWidget(resetPasswordButton_);
    freezeUserButton_ = button(QStringLiteral("冻结账号"), actions, QStringLiteral("freezeUserButton"));
    actionsLayout->addWidget(freezeUserButton_);
    // 删除账号：只允许单条删除，按钮在选中一条记录后才可用（危险的次要操作）。
    deleteUserButton_ = button(QStringLiteral("删除账号"), actions, QStringLiteral("deleteUserButton"));
    deleteUserButton_->setProperty("danger", true);
    actionsLayout->addWidget(deleteUserButton_);
    batchToggleButton_ = button(QStringLiteral("批量冻结/解冻"), actions, QStringLiteral("batchToggleButton"));
    actionsLayout->addWidget(batchToggleButton_);
    actionsLayout->addStretch();
    actionsLayout->addWidget(button(QStringLiteral("导出 CSV"), actions, QStringLiteral("exportUsersButton")));
    actionHintLabel_ = new QLabel(QStringLiteral("先选择一条用户记录（可 Ctrl 多选）"), actions);
    actionHintLabel_->setObjectName(QStringLiteral("pageHint"));
    actionsLayout->addWidget(actionHintLabel_);
    usersLayout->addWidget(actions);
    pages_->addWidget(usersPage);

    // ===== 在线用户管理页 =====
    auto* onlinePage = new QWidget(pages_);
    auto* onlineLayout = new QVBoxLayout(onlinePage);
    onlineLayout->setContentsMargins(24, 20, 24, 18);
    onlineLayout->setSpacing(14);
    auto* onlineHead = new QHBoxLayout();
    onlineHead->addWidget(title(QStringLiteral("在线用户管理"), onlinePage));
    auto* onlineHint = new QLabel(QStringLiteral("显示当前 TCP 连接的登录会话"), onlinePage);
    onlineHint->setObjectName(QStringLiteral("pageSubtitle"));
    onlineHead->addWidget(onlineHint);
    onlineHead->addStretch();
    onlineHead->addWidget(button(QStringLiteral("刷新列表"), onlinePage, QStringLiteral("refreshOnlineButton")));
    onlineLayout->addLayout(onlineHead);
    auto* onlinePanel = panel(onlinePage);
    auto* onlinePanelLayout = new QVBoxLayout(onlinePanel);
    onlinePanelLayout->setContentsMargins(0, 0, 0, 0);
    onlineTable_ = new QTableWidget(onlinePanel);
    onlineTable_->setObjectName(QStringLiteral("onlineTable"));
    onlineTable_->setColumnCount(5);
    onlineTable_->setHorizontalHeaderLabels({QStringLiteral("Session ID"), QStringLiteral("用户名"), QStringLiteral("角色"), QStringLiteral("连接状态"), QStringLiteral("来源地址")});
    onlineTable_->horizontalHeader()->setStretchLastSection(true);
    onlineTable_->verticalHeader()->setVisible(false);
    onlineTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    onlineTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    onlineTable_->setFocusPolicy(Qt::NoFocus);
    onlineTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    onlinePanelLayout->addWidget(onlineTable_, 1);
    auto* onlineFooter = new QFrame(onlinePanel);
    onlineFooter->setObjectName(QStringLiteral("tableFooter"));
    onlineFooter->setFixedHeight(38);
    auto* onlineFooterLayout = new QHBoxLayout(onlineFooter);
    onlineFooterLayout->setContentsMargins(12, 0, 12, 0);
    onlineFooterLayout->addWidget(new QLabel(QStringLiteral("仅管理员可以强制结束其他会话"), onlineFooter));
    onlinePanelLayout->addWidget(onlineFooter);
    onlineLayout->addWidget(onlinePanel, 1);
    auto* onlineActions = new QFrame(onlinePage);
    onlineActions->setObjectName(QStringLiteral("actionBar"));
    onlineActions->setFixedHeight(58);
    auto* onlineActionsLayout = new QHBoxLayout(onlineActions);
    onlineActionsLayout->setContentsMargins(14, 0, 14, 0);
    onlineActionsLayout->addWidget(new QLabel(QStringLiteral("会话操作"), onlineActions));
    onlineActionsLayout->addWidget(button(QStringLiteral("刷新列表"), onlineActions, QStringLiteral("refreshOnlineActionButton")));
    auto* offlineAction = button(QStringLiteral("强制踢下线"), onlineActions, QStringLiteral("forceOfflineButton"));
    offlineAction->setEnabled(false);
    forceOfflineButton_ = offlineAction;
    onlineActionsLayout->addWidget(offlineAction);
    onlineActionsLayout->addStretch();
    onlineActionsLayout->addWidget(new QLabel(QStringLiteral("选中会话后结束连接身份"), onlineActions));
    onlineLayout->addWidget(onlineActions);
    pages_->addWidget(onlinePage);

    // ===== 审计日志页 =====
    auto* auditPage = new QWidget(pages_);
    auto* auditLayout = new QVBoxLayout(auditPage);
    auditLayout->setContentsMargins(24, 20, 24, 18);
    auditLayout->setSpacing(14);
    auto* auditHead = new QHBoxLayout();
    auditHead->addWidget(title(QStringLiteral("审计日志"), auditPage));
    auto* auditHint = new QLabel(QStringLiteral("管理员操作与登录的完整留痕"), auditPage);
    auditHint->setObjectName(QStringLiteral("pageSubtitle"));
    auditHead->addWidget(auditHint);
    auditHead->addStretch();
    auditHead->addWidget(button(QStringLiteral("刷新列表"), auditPage, QStringLiteral("refreshAuditButton")));
    auditLayout->addLayout(auditHead);
    auto* auditPanel = panel(auditPage);
    auto* auditPanelLayout = new QVBoxLayout(auditPanel);
    auditPanelLayout->setContentsMargins(0, 0, 0, 0);
    auditPanelLayout->setSpacing(0);
    auditTable_ = new QTableWidget(auditPanel);
    auditTable_->setObjectName(QStringLiteral("auditTable"));
    auditTable_->setColumnCount(6);
    auditTable_->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("操作人"), QStringLiteral("动作"), QStringLiteral("对象"), QStringLiteral("结果"), QStringLiteral("来源 IP")});
    auditTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    auditTable_->horizontalHeader()->resizeSection(0, 150);
    auditTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    auditTable_->horizontalHeader()->resizeSection(1, 110);
    auditTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    auditTable_->horizontalHeader()->resizeSection(2, 150);
    auditTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    auditTable_->horizontalHeader()->resizeSection(3, 110);
    auditTable_->horizontalHeader()->setStretchLastSection(true);
    auditTable_->verticalHeader()->setVisible(false);
    auditTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auditTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    auditTable_->setFocusPolicy(Qt::NoFocus);
    auditTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auditTable_->setWordWrap(false);
    auditPanelLayout->addWidget(auditTable_, 1);
    auto* auditFooter = new QFrame(auditPanel);
    auditFooter->setObjectName(QStringLiteral("tableFooter"));
    auditFooter->setFixedHeight(38);
    auto* auditFooterLayout = new QHBoxLayout(auditFooter);
    auditFooterLayout->setContentsMargins(12, 0, 12, 0);
    auditFooterLabel_ = new QLabel(QStringLiteral("共 0 条记录"), auditFooter);
    auditFooterLabel_->setObjectName(QStringLiteral("pageHint"));
    auditFooterLayout->addWidget(auditFooterLabel_);
    auditFooterLayout->addStretch();
    auditPreviousButton_ = button(QStringLiteral("上一页"), auditFooter, QStringLiteral("auditPreviousButton"));
    auditFooterLayout->addWidget(auditPreviousButton_);
    auditPageLabel_ = new QLabel(QStringLiteral("第 1 / 1 页"), auditFooter);
    auditFooterLayout->addWidget(auditPageLabel_);
    auditNextButton_ = button(QStringLiteral("下一页"), auditFooter, QStringLiteral("auditNextButton"));
    auditFooterLayout->addWidget(auditNextButton_);
    auditPanelLayout->addWidget(auditFooter);
    auditLayout->addWidget(auditPanel, 1);
    pages_->addWidget(auditPage);

    // ===== 服务端监控页 =====
    auto* monitorPage = new QWidget(pages_);
    auto* monitorLayout = new QVBoxLayout(monitorPage);
    monitorLayout->setContentsMargins(24, 20, 24, 18);
    monitorLayout->setSpacing(14);
    auto* monitorHead = new QHBoxLayout();
    monitorHead->addWidget(title(QStringLiteral("服务端监控"), monitorPage));
    statRttLabel_ = new QLabel(QStringLiteral("心跳延迟：-"), monitorPage);
    statRttLabel_->setObjectName(QStringLiteral("pageSubtitle"));
    monitorHead->addWidget(statRttLabel_);
    monitorHead->addStretch();
    monitorHead->addWidget(button(QStringLiteral("刷新指标"), monitorPage, QStringLiteral("refreshStatsButton")));
    monitorLayout->addLayout(monitorHead);

    auto* monitorPanel = panel(monitorPage);
    auto* monitorForm = new QFormLayout(monitorPanel);
    monitorForm->setContentsMargins(18, 16, 18, 16);
    monitorForm->setSpacing(12);
    const auto monitorRow = [monitorPanel]() {
        auto* value = new QLabel(QStringLiteral("-"), monitorPanel);
        value->setObjectName(QStringLiteral("monitorValue"));
        return value;
    };
    statUptimeLabel_ = monitorRow();
    statConnectionsLabel_ = monitorRow();
    statTotalConnectionsLabel_ = monitorRow();
    statThreadsLabel_ = monitorRow();
    statStorageLabel_ = monitorRow();
    statSessionsLabel_ = monitorRow();
    monitorForm->addRow(QStringLiteral("运行时长"), statUptimeLabel_);
    monitorForm->addRow(QStringLiteral("当前 TCP 连接"), statConnectionsLabel_);
    monitorForm->addRow(QStringLiteral("累计接受连接"), statTotalConnectionsLabel_);
    monitorForm->addRow(QStringLiteral("IO 线程 / 业务线程"), statThreadsLabel_);
    monitorForm->addRow(QStringLiteral("存储后端"), statStorageLabel_);
    monitorForm->addRow(QStringLiteral("注册会话数"), statSessionsLabel_);
    auto* monitorHint = new QLabel(QStringLiteral("指标来自 STATS 协议（管理员权限），由服务端实时收集。"), monitorPanel);
    monitorHint->setObjectName(QStringLiteral("pageHint"));
    monitorForm->addRow(monitorHint);
    monitorLayout->addWidget(monitorPanel);
    monitorLayout->addStretch();
    pages_->addWidget(monitorPage);

    // 个人中心：导航之外的隐藏页（index 5），点击左下角头像进入。
    buildProfilePage();

    // ===== 信号连接 =====
    const auto emitUsersQuery = [this](int page) {
        const auto keyword = userSearchEdit_ ? userSearchEdit_->text().trimmed() : QString{};
        QString status;
        if (userStatusFilter_) {
            const auto text = userStatusFilter_->currentText();
            if (text == QStringLiteral("正常")) status = QStringLiteral("active");
            else if (text == QStringLiteral("已冻结")) status = QStringLiteral("frozen");
        }
        emit usersQueryRequested(page, keyword, status);
    };
    connect(findChild<QPushButton*>(QStringLiteral("refreshUsersButton")), &QPushButton::clicked, this, [emitUsersQuery]() { emitUsersQuery(1); });
    connect(searchButton, &QPushButton::clicked, this, [emitUsersQuery]() { emitUsersQuery(1); });
    connect(userSearchEdit_, &QLineEdit::returnPressed, this, [emitUsersQuery]() { emitUsersQuery(1); });
    connect(userStatusFilter_, &QComboBox::currentIndexChanged, this, [emitUsersQuery]() { emitUsersQuery(1); });
    connect(previousPageButton_, &QPushButton::clicked, this, [this, emitUsersQuery]() {
        if (usersPage_ > 1) emitUsersQuery(usersPage_ - 1);
    });
    connect(nextPageButton_, &QPushButton::clicked, this, [this, emitUsersQuery]() {
        if (usersPage_ < pageCount(usersTotal_, usersPageSize_)) emitUsersQuery(usersPage_ + 1);
    });

    connect(navigation_, &QListWidget::currentRowChanged, this, &DashboardWidget::switchPage);
    connect(newUserButton, &QPushButton::clicked, this, &DashboardWidget::showRegisterDialog);    connect(resetPasswordButton_, &QPushButton::clicked, this, [this]() {
        const auto username = selectedUsername();
        if (username.isEmpty()) return;
        if (QMessageBox::question(this, QStringLiteral("重置密码"),
                                  QStringLiteral("确定为用户“%1”生成新的随机密码吗？\n新密码仅显示一次，请妥善转达。").arg(username),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            emit resetPasswordRequested(username);
        }
    });
    connect(freezeUserButton_, &QPushButton::clicked, this, [this]() {
        const auto username = selectedUsername();
        if (username.isEmpty()) return;
        const auto selectedRows = usersTable_->selectionModel()->selectedRows();
        if (selectedRows.isEmpty()) return;
        const auto* statusItem = usersTable_->item(selectedRows.front().row(), 3);
        const bool frozen = statusItem && statusItem->data(Qt::UserRole).toString() == QStringLiteral("frozen");
        const auto action = frozen ? QStringLiteral("解冻") : QStringLiteral("冻结");
        if (QMessageBox::question(this, action + QStringLiteral("账号"),
                                  QStringLiteral("确定%1用户“%2”吗？").arg(action, username),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            emit setUserStatusRequested(username, !frozen);
        }
    });
    connect(usersTable_, &QTableWidget::itemSelectionChanged, this, &DashboardWidget::updateSelectionActions);
    connect(deleteUserButton_, &QPushButton::clicked, this, [this]() {
        const auto username = selectedUsername();
        if (username.isEmpty() || currentRoleCode_ != QStringLiteral("admin")) return;
        if (username == currentUsername_) {
            QMessageBox::warning(this, QStringLiteral("删除账号"), QStringLiteral("不能删除当前登录的账号。"));
            return;
        }
        // 二次确认：明确“不可恢复”，并提示优先冻结（离职场景通常只需停用）。
        QMessageBox box(this);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle(QStringLiteral("删除账号"));
        box.setText(QStringLiteral("确定永久删除账号“%1”吗？").arg(username));
        box.setInformativeText(QStringLiteral("删除后该账号的登录凭据会被移除，且无法恢复。\n如果是人员离职、只是不再允许登录，建议改用「冻结账号」。"));
        auto* confirm = box.addButton(QStringLiteral("确认删除"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == confirm) emit deleteUserRequested(username);
    });
    connect(findChild<QPushButton*>(QStringLiteral("refreshOnlineActionButton")), &QPushButton::clicked, this, &DashboardWidget::onlineSessionsRequested);
    connect(findChild<QPushButton*>(QStringLiteral("refreshOnlineButton")), &QPushButton::clicked, this, &DashboardWidget::onlineSessionsRequested);
    connect(navigation_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row == 0) emit overviewRefreshRequested();
        if (row == 2) emit onlineSessionsRequested();
        if (row == 3) emit auditLogsRequested(1);
        if (row == 4) emit statsRequested();
        if (row == 5) { emit accountInfoRequested(); emit myActivityRequested(); emit mySessionsRequested(); }
    });
    const auto emitForceLogout = [this]() {
        const auto sessionId = selectedOnlineSessionId();
        if (!sessionId.isEmpty()) emit forceLogoutRequested(sessionId);
    };
    connect(forceOfflineButton_, &QPushButton::clicked, this, emitForceLogout);
    connect(onlineTable_, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (forceOfflineButton_) forceOfflineButton_->setEnabled(!selectedOnlineSessionId().isEmpty() && currentRoleCode_ == QStringLiteral("admin"));
    });
    // 批量冻结/解冻：按每行当前状态决定目标状态？统一以多数为准会误操作，
    // 因此这里询问用户要冻结还是解冻。
    connect(batchToggleButton_, &QPushButton::clicked, this, [this]() {
        const auto usernames = selectedUsernames();
        if (usernames.isEmpty() || currentRoleCode_ != QStringLiteral("admin")) return;
        QStringList items;
        for (const auto& name : usernames) items += QStringLiteral("• %1").arg(name);
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("批量操作"));
        box.setText(QStringLiteral("已选择 %1 个账号：\n%2").arg(usernames.size()).arg(items.join(QLatin1Char('\n'))));
        auto* freeze = box.addButton(QStringLiteral("批量冻结"), QMessageBox::AcceptRole);
        auto* unfreeze = box.addButton(QStringLiteral("批量解冻"), QMessageBox::AcceptRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();
        if (box.clickedButton() == freeze) emit batchSetStatusRequested(usernames, true);
        else if (box.clickedButton() == unfreeze) emit batchSetStatusRequested(usernames, false);
    });
    connect(findChild<QPushButton*>(QStringLiteral("exportUsersButton")), &QPushButton::clicked, this, &DashboardWidget::exportUsersRequested);
    connect(account, &QPushButton::clicked, this, &DashboardWidget::openProfilePage);

    connect(findChild<QPushButton*>(QStringLiteral("refreshAuditButton")), &QPushButton::clicked, this, [this]() { emit auditLogsRequested(1); });
    connect(auditPreviousButton_, &QPushButton::clicked, this, [this]() {
        if (auditPage_ > 1) emit auditLogsRequested(auditPage_ - 1);
    });
    connect(auditNextButton_, &QPushButton::clicked, this, [this]() {
        if (auditPage_ < pageCount(auditTotal_, auditPageSize_)) emit auditLogsRequested(auditPage_ + 1);
    });
    // 初始状态：空数据 + 分页按钮禁用。
    applyUsersPagination(0, 1, 10);
    applyAuditPagination(0, 1, 10);
    updateSelectionActions();
}

void DashboardWidget::switchPage(int row) {
    if (pages_ && row >= 0 && row < pages_->count()) pages_->setCurrentIndex(row);
}

QString DashboardWidget::selectedOnlineSessionId() const {
    if (!onlineTable_) return {};
    const auto rows = onlineTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* item = onlineTable_->item(rows.front().row(), 0);
    return item ? item->data(Qt::UserRole).toString() : QString{};
}

QString DashboardWidget::selectedUsername() const {
    if (!usersTable_) return {};
    const auto rows = usersTable_->selectionModel()->selectedRows();
    if (rows.isEmpty()) return {};
    const auto* item = usersTable_->item(rows.front().row(), 1);
    return item ? item->text() : QString{};
}

QStringList DashboardWidget::selectedUsernames() const {
    QStringList result;
    if (!usersTable_) return result;
    for (const auto& index : usersTable_->selectionModel()->selectedRows()) {
        const auto* item = usersTable_->item(index.row(), 1);
        if (item && !item->text().isEmpty()) result += item->text();
    }
    return result;
}

void DashboardWidget::buildOverviewPage() {
    auto* overviewPage = new QWidget(pages_);
    auto* layout = new QVBoxLayout(overviewPage);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(14);

    auto* head = new QHBoxLayout();
    head->addWidget(title(QStringLiteral("数据总览"), overviewPage));
    overviewHintLabel_ = new QLabel(QStringLiteral("打开页面自动刷新"), overviewPage);
    overviewHintLabel_->setObjectName(QStringLiteral("pageSubtitle"));
    head->addWidget(overviewHintLabel_);
    head->addStretch();
    auto* refresh = button(QStringLiteral("刷新数据"), overviewPage, QStringLiteral("refreshOverviewButton"));
    refresh->setProperty("primary", true);
    head->addWidget(refresh);
    layout->addLayout(head);

    // 四张统计卡片。
    auto* cards = new QHBoxLayout();
    cards->setSpacing(12);
    const auto makeCard = [this, cards, overviewPage](const QString& caption, QLabel** valueLabel, const QString& objectName) {
        auto* card = new QFrame(overviewPage);
        card->setObjectName(QStringLiteral("statCard"));
        auto* cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(18, 14, 18, 14);
        cardLayout->setSpacing(4);
        auto* value = new QLabel(QStringLiteral("0"), card);
        value->setObjectName(objectName);
        value->setAlignment(Qt::AlignLeft);
        auto* cap = new QLabel(caption, card);
        cap->setObjectName(QStringLiteral("statCaption"));
        cardLayout->addWidget(value);
        cardLayout->addWidget(cap);
        cards->addWidget(card, 1);
        *valueLabel = value;
    };
    makeCard(QStringLiteral("账号总数"), &statTotalLabel_, QStringLiteral("statValueTotal"));
    makeCard(QStringLiteral("正常账号"), &statActiveLabel_, QStringLiteral("statValueActive"));
    makeCard(QStringLiteral("冻结账号"), &statFrozenLabel_, QStringLiteral("statValueFrozen"));
    makeCard(QStringLiteral("在线会话"), &statOnlineLabel_, QStringLiteral("statValueOnline"));
    layout->addLayout(cards);

    auto* panelFrame = panel(overviewPage);
    auto* panelLayout = new QVBoxLayout(panelFrame);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    auto* caption = new QLabel(QStringLiteral("最近动态"), panelFrame);
    caption->setObjectName(QStringLiteral("panelTitle"));
    caption->setContentsMargins(12, 8, 12, 8);
    panelLayout->addWidget(caption);
    overviewActivityTable_ = new QTableWidget(panelFrame);
    overviewActivityTable_->setObjectName(QStringLiteral("overviewActivityTable"));
    overviewActivityTable_->setColumnCount(5);
    overviewActivityTable_->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("操作人"), QStringLiteral("动作"), QStringLiteral("对象"), QStringLiteral("结果")});
    overviewActivityTable_->horizontalHeader()->setStretchLastSection(true);
    overviewActivityTable_->verticalHeader()->setVisible(false);
    overviewActivityTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    overviewActivityTable_->setSelectionMode(QAbstractItemView::NoSelection);
    overviewActivityTable_->setFocusPolicy(Qt::NoFocus);
    overviewActivityTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    overviewActivityTable_->setWordWrap(false);
    panelLayout->addWidget(overviewActivityTable_, 1);
    layout->addWidget(panelFrame, 1);
    pages_->addWidget(overviewPage);

    connect(refresh, &QPushButton::clicked, this, &DashboardWidget::overviewRefreshRequested);
}

void DashboardWidget::buildProfilePage() {
    auto* profilePage = new QWidget(pages_);
    auto* layout = new QHBoxLayout(profilePage);
    layout->setContentsMargins(24, 20, 24, 18);
    layout->setSpacing(16);

    // 左列：账号信息 + 修改密码。
    auto* leftColumn = new QVBoxLayout();
    auto* infoPanel = panel(profilePage);
    infoPanel->setMaximumWidth(430);
    auto* infoLayout = new QVBoxLayout(infoPanel);
    infoLayout->setContentsMargins(18, 16, 18, 16);
    infoLayout->setSpacing(10);
    auto* identityRow = new QHBoxLayout();
    profileAvatarLabel_ = new QLabel(QStringLiteral("A"), infoPanel);
    profileAvatarLabel_->setObjectName(QStringLiteral("profileAvatar"));
    profileAvatarLabel_->setAlignment(Qt::AlignCenter);
    profileAvatarLabel_->setFixedSize(56, 56);
    auto* identityText = new QVBoxLayout();
    identityText->setSpacing(2);
    profileNameLabel_ = new QLabel(currentUsername_, infoPanel);
    profileNameLabel_->setObjectName(QStringLiteral("profileName"));
    profileRoleLabel_ = new QLabel(currentRole_, infoPanel);
    profileRoleLabel_->setObjectName(QStringLiteral("pageSubtitle"));
    identityText->addWidget(profileNameLabel_);
    identityText->addWidget(profileRoleLabel_);
    identityRow->addWidget(profileAvatarLabel_);
    identityRow->addSpacing(8);
    identityRow->addLayout(identityText);
    identityRow->addStretch();
    infoLayout->addLayout(identityRow);

    // 账号信息（WHOAMI 数据源）：普通用户也能完整看到自己的会话状态。
    auto* accountForm = new QFormLayout();
    accountForm->setSpacing(8);
    profileUserIdLabel_ = new QLabel(QStringLiteral("-"), infoPanel);
    profileUserIdLabel_->setObjectName(QStringLiteral("profileUserId"));
    profileStatusLabel_ = new QLabel(QStringLiteral("-"), infoPanel);
    profileStatusLabel_->setObjectName(QStringLiteral("profileStatus"));
    profileIpLabel_ = new QLabel(QStringLiteral("-"), infoPanel);
    profileIpLabel_->setObjectName(QStringLiteral("profileIp"));
    profileLoginTimeLabel_ = new QLabel(QStringLiteral("-"), infoPanel);
    profileLoginTimeLabel_->setObjectName(QStringLiteral("profileLoginTime"));
    profileActiveLabel_ = new QLabel(QStringLiteral("-"), infoPanel);
    profileActiveLabel_->setObjectName(QStringLiteral("profileActive"));
    accountForm->addRow(QStringLiteral("账号 ID"), profileUserIdLabel_);
    accountForm->addRow(QStringLiteral("账号状态"), profileStatusLabel_);
    accountForm->addRow(QStringLiteral("登录 IP"), profileIpLabel_);
    accountForm->addRow(QStringLiteral("登录时间"), profileLoginTimeLabel_);
    accountForm->addRow(QStringLiteral("最近活跃"), profileActiveLabel_);
    // 上次改密时间：从本人的审计记录里取最近一条 user.change_password。
    profilePasswordChangedLabel_ = new QLabel(QStringLiteral("-"), infoPanel);
    profilePasswordChangedLabel_->setObjectName(QStringLiteral("profilePasswordChanged"));
    accountForm->addRow(QStringLiteral("上次改密"), profilePasswordChangedLabel_);
    infoLayout->addLayout(accountForm);

    auto* infoDivider = new QLabel(infoPanel);
    infoDivider->setFixedHeight(1);
    infoDivider->setStyleSheet(QStringLiteral("background: rgba(0,0,0,0.08);"));
    infoLayout->addWidget(infoDivider);

    auto* form = new QFormLayout();
    form->setSpacing(10);
    profileOldPasswordEdit_ = new QLineEdit(infoPanel);
    profileOldPasswordEdit_->setEchoMode(QLineEdit::Password);
    profileOldPasswordEdit_->setPlaceholderText(QStringLiteral("必填：当前密码"));
    profileNewPasswordEdit_ = new QLineEdit(infoPanel);
    profileNewPasswordEdit_->setEchoMode(QLineEdit::Password);
    profileNewPasswordEdit_->setPlaceholderText(QStringLiteral("至少 6 位且不与旧密码相同"));
    profileConfirmEdit_ = new QLineEdit(infoPanel);
    profileConfirmEdit_->setEchoMode(QLineEdit::Password);
    profileConfirmEdit_->setPlaceholderText(QStringLiteral("再输入一次新密码"));
    form->addRow(QStringLiteral("旧密码"), profileOldPasswordEdit_);
    form->addRow(QStringLiteral("新密码"), profileNewPasswordEdit_);
    form->addRow(QStringLiteral("确认新密码"), profileConfirmEdit_);
    infoLayout->addLayout(form);
    auto* buttonRow = new QHBoxLayout();
    auto* saveButton = button(QStringLiteral("保存修改"), infoPanel, QStringLiteral("saveProfilePasswordButton"));
    saveButton->setProperty("primary", true);
    buttonRow->addWidget(saveButton);
    buttonRow->addStretch();
    auto* logoutButton = button(QStringLiteral("退出账号"), infoPanel, QStringLiteral("logoutButton"));
    buttonRow->addWidget(logoutButton);
    infoLayout->addLayout(buttonRow);
    auto* hint = new QLabel(QStringLiteral("修改密码需验证旧密码；用户名和角色由服务端维护。"), infoPanel);
    hint->setObjectName(QStringLiteral("pageHint"));
    hint->setWordWrap(true);
    infoLayout->addWidget(hint);
    leftColumn->addWidget(infoPanel);
    leftColumn->addStretch();
    layout->addLayout(leftColumn, 1);

    // 右列：上「我的登录设备」，下「我的最近操作」。
    auto* rightColumn = new QVBoxLayout();
    rightColumn->setSpacing(14);

    auto* sessionsPanel = panel(profilePage);
    auto* sessionsLayout = new QVBoxLayout(sessionsPanel);
    sessionsLayout->setContentsMargins(0, 0, 0, 0);
    auto* sessionsHead = new QHBoxLayout();
    auto* sessionsCaption = new QLabel(QStringLiteral("我的登录设备"), sessionsPanel);
    sessionsCaption->setObjectName(QStringLiteral("panelTitle"));
    sessionsCaption->setContentsMargins(12, 8, 12, 8);
    sessionsHead->addWidget(sessionsCaption);
    sessionsHead->addStretch();
    logoutDeviceButton_ = button(QStringLiteral("退出该设备"), sessionsPanel, QStringLiteral("logoutDeviceButton"));
    logoutDeviceButton_->setEnabled(false);
    sessionsHead->addWidget(logoutDeviceButton_);
    sessionsHead->addWidget(button(QStringLiteral("刷新"), sessionsPanel, QStringLiteral("refreshMySessionsButton")));
    sessionsLayout->addLayout(sessionsHead);
    mySessionsTable_ = new QTableWidget(sessionsPanel);
    mySessionsTable_->setObjectName(QStringLiteral("mySessionsTable"));
    mySessionsTable_->setColumnCount(5);
    mySessionsTable_->setHorizontalHeaderLabels({QStringLiteral("设备/会话"), QStringLiteral("来源地址"),
                                                 QStringLiteral("登录时间"), QStringLiteral("最近活跃"), QStringLiteral("当前")});
    mySessionsTable_->horizontalHeader()->setStretchLastSection(true);
    mySessionsTable_->verticalHeader()->setVisible(false);
    mySessionsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    mySessionsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    mySessionsTable_->setFocusPolicy(Qt::NoFocus);
    mySessionsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mySessionsTable_->setMinimumHeight(150);
    sessionsLayout->addWidget(mySessionsTable_, 1);
    auto* sessionsHint = new QLabel(QStringLiteral("这里只显示你自己的会话；在别的电脑登录后，可以在这里把旧设备踢下线。"), sessionsPanel);
    sessionsHint->setObjectName(QStringLiteral("pageHint"));
    sessionsHint->setContentsMargins(12, 2, 12, 8);
    sessionsHint->setWordWrap(true);
    sessionsLayout->addWidget(sessionsHint);
    rightColumn->addWidget(sessionsPanel);

    auto* rightPanel = panel(profilePage);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto* rightHead = new QHBoxLayout();
    auto* rightCaption = new QLabel(QStringLiteral("我的最近操作"), rightPanel);
    rightCaption->setObjectName(QStringLiteral("panelTitle"));
    rightCaption->setContentsMargins(12, 8, 12, 8);
    rightHead->addWidget(rightCaption);
    rightHead->addStretch();
    auto* exportMine = button(QStringLiteral("导出记录"), rightPanel, QStringLiteral("exportMyActivityButton"));
    rightHead->addWidget(exportMine);
    auto* refreshMine = button(QStringLiteral("刷新"), rightPanel, QStringLiteral("refreshMyActivityButton"));
    rightHead->addWidget(refreshMine);
    rightLayout->addLayout(rightHead);
    myActivityTable_ = new QTableWidget(rightPanel);
    myActivityTable_->setObjectName(QStringLiteral("myActivityTable"));
    myActivityTable_->setColumnCount(4);
    myActivityTable_->setHorizontalHeaderLabels({QStringLiteral("时间"), QStringLiteral("动作"), QStringLiteral("对象"), QStringLiteral("结果")});
    myActivityTable_->horizontalHeader()->setStretchLastSection(true);
    myActivityTable_->verticalHeader()->setVisible(false);
    myActivityTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    myActivityTable_->setSelectionMode(QAbstractItemView::NoSelection);
    myActivityTable_->setFocusPolicy(Qt::NoFocus);
    myActivityTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    myActivityTable_->setWordWrap(false);
    rightLayout->addWidget(myActivityTable_, 1);
    rightColumn->addWidget(rightPanel, 1);
    layout->addLayout(rightColumn, 2);

    pages_->addWidget(profilePage);

    connect(saveButton, &QPushButton::clicked, this, [this]() {
        const auto oldPassword = profileOldPasswordEdit_->text();
        const auto newPassword = profileNewPasswordEdit_->text();
        if (oldPassword.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("修改密码"), QStringLiteral("请输入当前密码。"));
            return;
        }
        if (newPassword.size() < 6) {
            QMessageBox::warning(this, QStringLiteral("修改密码"), QStringLiteral("新密码至少需要 6 位。"));
            return;
        }
        if (newPassword != profileConfirmEdit_->text()) {
            QMessageBox::warning(this, QStringLiteral("修改密码"), QStringLiteral("两次输入的新密码不一致。"));
            return;
        }
        emit changePasswordRequested(currentUsername_, oldPassword, newPassword);
    });
    connect(logoutButton, &QPushButton::clicked, this, &DashboardWidget::logoutRequested);
    connect(refreshMine, &QPushButton::clicked, this, &DashboardWidget::myActivityRequested);
    connect(exportMine, &QPushButton::clicked, this, &DashboardWidget::exportMyActivityRequested);
    connect(findChild<QPushButton*>(QStringLiteral("refreshMySessionsButton")), &QPushButton::clicked,
            this, &DashboardWidget::mySessionsRequested);
    connect(logoutDeviceButton_, &QPushButton::clicked, this, [this]() {
        if (!mySessionsTable_) return;
        const auto rows = mySessionsTable_->selectionModel()->selectedRows();
        if (rows.isEmpty()) return;
        const auto* item = mySessionsTable_->item(rows.front().row(), 0);
        if (!item) return;
        const auto sessionId = item->data(Qt::UserRole).toString();
        if (sessionId.isEmpty()) return;
        const auto peer = mySessionsTable_->item(rows.front().row(), 1);
        if (QMessageBox::question(this, QStringLiteral("退出设备"),
                                  QStringLiteral("确定让设备「%1」下线吗？该设备上的会话会立即失效，需要重新登录。")
                                      .arg(peer ? peer->text() : sessionId),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            emit logoutDeviceRequested(sessionId);
        }
    });
    connect(mySessionsTable_, &QTableWidget::itemSelectionChanged, this, [this]() {
        if (!logoutDeviceButton_ || !mySessionsTable_) return;
        const auto rows = mySessionsTable_->selectionModel()->selectedRows();
        bool canLogout = false;
        if (!rows.isEmpty()) {
            const auto* item = mySessionsTable_->item(rows.front().row(), 0);
            const auto* currentItem = mySessionsTable_->item(rows.front().row(), 4);
            const bool isCurrent = currentItem && currentItem->text() == QStringLiteral("本机");
            canLogout = item && !item->data(Qt::UserRole).toString().isEmpty() && !isCurrent;
        }
        logoutDeviceButton_->setEnabled(canLogout);
    });
}

void DashboardWidget::updateSelectionActions() {
    const auto rows = usersTable_ ? usersTable_->selectionModel()->selectedRows() : QModelIndexList{};
    const bool selected = !selectedUsername().isEmpty();
    const bool admin = currentRoleCode_ == QStringLiteral("admin");
    if (resetPasswordButton_) resetPasswordButton_->setEnabled(selected && admin);
    if (freezeUserButton_) freezeUserButton_->setEnabled(selected && admin);
    // 删除只接受单条：多选时禁用，避免误删一批账号。
    if (deleteUserButton_) {
        deleteUserButton_->setEnabled(selected && admin && rows.size() == 1 && selectedUsername() != currentUsername_);
    }
    if (actionHintLabel_) actionHintLabel_->setText(selected ? QStringLiteral("已选择：%1").arg(selectedUsername()) : QStringLiteral("先选择一条用户记录"));
}

void DashboardWidget::setActionHint(const QString& message, bool error) {
    if (!actionHintLabel_) return;
    actionHintLabel_->setText(message);
    actionHintLabel_->setStyleSheet(error ? QStringLiteral("color: #c0392b; font-weight: 600;") : QString());
}

void DashboardWidget::clearActionHint() {
    if (!actionHintLabel_) return;
    actionHintLabel_->setStyleSheet(QString());
    updateSelectionActions();
}

void DashboardWidget::showRegisterDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("新增用户"));
    dialog.setMinimumWidth(360);
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();
    auto* username = new QLineEdit(&dialog);
    username->setPlaceholderText(QStringLiteral("3-32 位字母、数字、下划线或连字符"));
    auto* password = new QLineEdit(&dialog);
    password->setEchoMode(QLineEdit::Password);
    password->setPlaceholderText(QStringLiteral("至少 6 位"));
    form->addRow(QStringLiteral("用户名"), username);
    form->addRow(QStringLiteral("密码"), password);
    // 超级管理员唯一：新增账号一律是普通用户，界面上不再提供角色选择。
    layout->addLayout(form);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("创建"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        emit registerRequested(username->text(), password->text(), QStringLiteral("user"));
    }
}

void DashboardWidget::openProfilePage() {
    if (!pages_) return;
    // 个人中心固定为第 6 页（导航“我的空间”与头像点击共用）。
    if (navigation_ && navigation_->currentRow() != 5) {
        // 选中导航会触发 switchPage 和数据拉取信号，避免重复请求。
        navigation_->setCurrentRow(5);
        return;
    }
    pages_->setCurrentIndex(5);
    emit accountInfoRequested();
    emit myActivityRequested();
}

QString DashboardWidget::formatDuration(qint64 seconds) const {
    if (seconds < 0) return QStringLiteral("-");
    const qint64 days = seconds / 86400;
    const qint64 hours = (seconds % 86400) / 3600;
    const qint64 minutes = (seconds % 3600) / 60;
    const qint64 secs = seconds % 60;
    if (days > 0) return QStringLiteral("%1天 %2小时 %3分").arg(days).arg(hours).arg(minutes);
    if (hours > 0) return QStringLiteral("%1小时 %2分 %3秒").arg(hours).arg(minutes).arg(secs);
    if (minutes > 0) return QStringLiteral("%1分 %2秒").arg(minutes).arg(secs);
    return QStringLiteral("%1 秒").arg(secs);
}

void DashboardWidget::setCurrentUser(const QString& username, const QString& role) {
    if (!username.trimmed().isEmpty()) currentUsername_ = username.trimmed();
    if (!role.trimmed().isEmpty()) currentRoleCode_ = role.trimmed();
    if (role == QStringLiteral("admin")) currentRole_ = QStringLiteral("超级管理员");
    else if (role == QStringLiteral("user")) currentRole_ = QStringLiteral("普通用户");
    else if (!role.trimmed().isEmpty()) currentRole_ = role.trimmed();
    if (auto* label = findChild<QLabel*>(QStringLiteral("accountName"))) label->setText(currentUsername_);
    if (auto* label = findChild<QLabel*>(QStringLiteral("accountRole"))) label->setText(currentRole_);
    if (auto* label = findChild<QLabel*>(QStringLiteral("accountAvatar"))) label->setText(currentUsername_.left(1).toUpper());
    if (profileNameLabel_) profileNameLabel_->setText(currentUsername_);
    if (profileRoleLabel_) profileRoleLabel_->setText(currentRole_);
    if (profileAvatarLabel_) profileAvatarLabel_->setText(currentUsername_.left(1).toUpper());
    if (navigation_) {
        const bool admin = currentRoleCode_ == QStringLiteral("admin");
        // row 0-4 为管理员页面；row 5「我的空间」是普通用户的唯一导航页，
        // 管理员也能通过它进入（与点头像等价）。
        for (int row = 0; row < navigation_->count(); ++row) {
            if (auto* item = navigation_->item(row)) item->setHidden(admin ? row == 5 : row != 5);
        }
        if (!admin && navigation_->currentRow() != 5) navigation_->setCurrentRow(5);
    }
}

void DashboardWidget::refreshCurrent() {
    if (!navigation_) return;
    const int row = navigation_->currentRow();
    if (row == 0) emit overviewRefreshRequested();
    else if (row == 2) emit onlineSessionsRequested();
    else if (row == 3) emit auditLogsRequested(1);
    else if (row == 4) emit statsRequested();
    else if (row == 5) { emit accountInfoRequested(); emit myActivityRequested(); emit mySessionsRequested(); }
}

void DashboardWidget::setOverviewStats(int totalUsers, int activeUsers, int frozenUsers, int onlineSessions) {
    if (statTotalLabel_) statTotalLabel_->setText(QString::number(totalUsers));
    if (statActiveLabel_) statActiveLabel_->setText(QString::number(activeUsers));
    if (statFrozenLabel_) statFrozenLabel_->setText(QString::number(frozenUsers));
    if (statOnlineLabel_) statOnlineLabel_->setText(QString::number(onlineSessions));
    if (overviewHintLabel_) overviewHintLabel_->setText(QStringLiteral("更新于 %1").arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss"))));
}

void DashboardWidget::setOverviewActivity(const QVector<AuditLogRow>& logs) {
    if (!overviewActivityTable_) return;
    overviewActivityTable_->setRowCount(0);
    for (const auto& log : logs) {
        const int row = overviewActivityTable_->rowCount();
        overviewActivityTable_->insertRow(row);
        overviewActivityTable_->setItem(row, 0, new QTableWidgetItem(log.time));
        overviewActivityTable_->setItem(row, 1, new QTableWidgetItem(log.actor));
        overviewActivityTable_->setItem(row, 2, new QTableWidgetItem(log.action));
        overviewActivityTable_->setItem(row, 3, new QTableWidgetItem(log.target.isEmpty() ? QStringLiteral("-") : log.target));
        overviewActivityTable_->setItem(row, 4, new QTableWidgetItem(log.result));
    }
}

void DashboardWidget::setServerStats(const ServerStatsRow& stats) {
    if (statUptimeLabel_) statUptimeLabel_->setText(formatDuration(stats.uptimeSeconds));
    if (statConnectionsLabel_) statConnectionsLabel_->setText(QString::number(stats.currentConnections));
    if (statTotalConnectionsLabel_) statTotalConnectionsLabel_->setText(QString::number(stats.totalConnections));
    if (statThreadsLabel_) {
        statThreadsLabel_->setText(QStringLiteral("%1 / %2").arg(stats.ioThreads).arg(stats.workerThreads));
    }
    if (statStorageLabel_) statStorageLabel_->setText(stats.storage.isEmpty() ? QStringLiteral("-") : stats.storage);
    if (statSessionsLabel_) statSessionsLabel_->setText(QString::number(stats.activeSessions));
    if (statRttLabel_) {
        statRttLabel_->setText(stats.heartbeatRttMs >= 0
                                   ? QStringLiteral("心跳延迟：%1 ms").arg(stats.heartbeatRttMs)
                                   : QStringLiteral("心跳延迟：-"));
    }
}

void DashboardWidget::setHeartbeatRtt(qint64 rttMs) {
    if (statRttLabel_) statRttLabel_->setText(QStringLiteral("心跳延迟：%1 ms").arg(rttMs));
}

void DashboardWidget::setMyActivity(const QVector<AuditLogRow>& logs) {
    if (!myActivityTable_) return;
    myActivityTable_->setRowCount(0);
    // 顺带刷新「上次改密」：取本人最近一条改密记录（按时间倒序，第一条命中即可）。
    lastPasswordChangeTime_.clear();
    for (const auto& log : logs) {
        const int row = myActivityTable_->rowCount();
        myActivityTable_->insertRow(row);
        myActivityTable_->setItem(row, 0, new QTableWidgetItem(log.time));
        myActivityTable_->setItem(row, 1, new QTableWidgetItem(log.action));
        myActivityTable_->setItem(row, 2, new QTableWidgetItem(log.target.isEmpty() ? QStringLiteral("-") : log.target));
        myActivityTable_->setItem(row, 3, new QTableWidgetItem(log.result));
        if (lastPasswordChangeTime_.isEmpty() && log.action == QStringLiteral("user.change_password")
            && log.result == QStringLiteral("success")) {
            lastPasswordChangeTime_ = log.time;
        }
    }
    if (profilePasswordChangedLabel_) {
        profilePasswordChangedLabel_->setText(lastPasswordChangeTime_.isEmpty()
                                                  ? QStringLiteral("暂无改密记录")
                                                  : lastPasswordChangeTime_);
    }
}

void DashboardWidget::setMySessions(const QVector<MySessionRow>& sessions) {
    if (!mySessionsTable_) return;
    const auto formatTime = [](qint64 seconds) {
        if (seconds <= 0) return QStringLiteral("-");
        return QDateTime::fromSecsSinceEpoch(seconds).toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    };
    mySessionsTable_->setRowCount(0);
    for (const auto& item : sessions) {
        const int row = mySessionsTable_->rowCount();
        mySessionsTable_->insertRow(row);
        // 会话 ID 是连接 fd，只作为内部标识展示，不写入 UserRole 之外的地方。
        auto* idItem = new QTableWidgetItem(QStringLiteral("会话 %1").arg(item.sessionId));
        idItem->setData(Qt::UserRole, item.sessionId);
        mySessionsTable_->setItem(row, 0, idItem);
        mySessionsTable_->setItem(row, 1, new QTableWidgetItem(item.peerAddress));
        mySessionsTable_->setItem(row, 2, new QTableWidgetItem(formatTime(item.authenticatedAt)));
        mySessionsTable_->setItem(row, 3, new QTableWidgetItem(formatTime(item.lastActiveAt)));
        mySessionsTable_->setItem(row, 4, new QTableWidgetItem(item.current ? QStringLiteral("本机") : QString{}));
    }
    if (logoutDeviceButton_) logoutDeviceButton_->setEnabled(false);
}

void DashboardWidget::setAccountInfo(const AccountInfoRow& info) {
    if (profileUserIdLabel_) profileUserIdLabel_->setText(info.userId > 0 ? QString::number(info.userId) : QStringLiteral("-"));
    if (profileStatusLabel_) {
        profileStatusLabel_->setText(info.status == QStringLiteral("frozen") ? QStringLiteral("已冻结")
                                                                            : QStringLiteral("正常"));
    }
    if (profileIpLabel_) profileIpLabel_->setText(info.peerAddress.isEmpty() ? QStringLiteral("-") : info.peerAddress);
    const auto formatTime = [](qint64 seconds) {
        if (seconds <= 0) return QStringLiteral("-");
        return QDateTime::fromSecsSinceEpoch(seconds).toString(QStringLiteral("yyyy-MM-dd hh:mm:ss"));
    };
    if (profileLoginTimeLabel_) profileLoginTimeLabel_->setText(formatTime(info.authenticatedAt));
    if (profileActiveLabel_) {
        // 优先用服务端时间计算“多久前活跃”，避免本机时钟偏差。
        qint64 now = info.serverTimeSeconds > 0 ? info.serverTimeSeconds
                                                : QDateTime::currentSecsSinceEpoch();
        const qint64 elapsed = info.lastActiveAt > 0 ? now - info.lastActiveAt : -1;
        profileActiveLabel_->setText(elapsed >= 0 ? QStringLiteral("%1 秒前").arg(elapsed) : QStringLiteral("-"));
    }
}

void DashboardWidget::applyUsersPagination(int total, int page, int pageSize) {
    usersTotal_ = total;
    usersPage_ = page;
    usersPageSize_ = pageSize > 0 ? pageSize : 10;
    const int pages = pageCount(total, usersPageSize_);
    if (userPageLabel_) userPageLabel_->setText(QStringLiteral("第 %1 / %2 页").arg(usersPage_).arg(pages));
    if (userFooterLabel_) userFooterLabel_->setText(QStringLiteral("共 %1 条记录，每页 %2 条").arg(total).arg(usersPageSize_));
    if (userCountLabel_) userCountLabel_->setText(QStringLiteral("共 %1 个账号").arg(total));
    if (previousPageButton_) previousPageButton_->setEnabled(usersPage_ > 1);
    if (nextPageButton_) nextPageButton_->setEnabled(usersPage_ < pages);
}

void DashboardWidget::setUsers(const QVector<UserRow>& users, int total, int page, int pageSize) {
    usersTable_->setRowCount(0);
    for (const auto& user : users) {
        const int row = usersTable_->rowCount();
        usersTable_->insertRow(row);
        usersTable_->setItem(row, 0, new QTableWidgetItem(QString::number(user.id)));
        usersTable_->setItem(row, 1, new QTableWidgetItem(user.username));
        usersTable_->setItem(row, 2, new QTableWidgetItem(user.role));
        auto* status = new QTableWidgetItem(user.status == QStringLiteral("frozen") ? QStringLiteral("已冻结") : QStringLiteral("正常"));
        status->setData(Qt::UserRole, user.status);
        usersTable_->setItem(row, 3, status);
    }
    applyUsersPagination(total, page, pageSize);
    if (!users.isEmpty()) {
        usersTable_->setCurrentCell(0, 0);
        usersTable_->selectRow(0);
    }
    updateSelectionActions();
}

void DashboardWidget::setOnlineSessions(const QVector<OnlineSessionRow>& sessions) {
    if (!onlineTable_) return;
    onlineTable_->setRowCount(0);
    for (const auto& session : sessions) {
        const int row = onlineTable_->rowCount();
        onlineTable_->insertRow(row);
        auto* id = new QTableWidgetItem(session.sessionId);
        id->setData(Qt::UserRole, session.sessionId);
        onlineTable_->setItem(row, 0, id);
        onlineTable_->setItem(row, 1, new QTableWidgetItem(session.username.isEmpty() ? QStringLiteral("未登录") : session.username));
        onlineTable_->setItem(row, 2, new QTableWidgetItem(session.role.isEmpty() ? QStringLiteral("-" ) : session.role));
        onlineTable_->setItem(row, 3, new QTableWidgetItem(session.authenticated ? QStringLiteral("在线") : QStringLiteral("已连接未登录")));
        onlineTable_->setItem(row, 4, new QTableWidgetItem(session.peerAddress.isEmpty() ? QStringLiteral("-") : session.peerAddress));
    }
    if (forceOfflineButton_) forceOfflineButton_->setEnabled(false);
}

void DashboardWidget::applyAuditPagination(int total, int page, int pageSize) {
    auditTotal_ = total;
    auditPage_ = page;
    auditPageSize_ = pageSize > 0 ? pageSize : 10;
    const int pages = pageCount(total, auditPageSize_);
    if (auditPageLabel_) auditPageLabel_->setText(QStringLiteral("第 %1 / %2 页").arg(auditPage_).arg(pages));
    if (auditFooterLabel_) auditFooterLabel_->setText(QStringLiteral("共 %1 条记录，每页 %2 条").arg(total).arg(auditPageSize_));
    if (auditPreviousButton_) auditPreviousButton_->setEnabled(auditPage_ > 1);
    if (auditNextButton_) auditNextButton_->setEnabled(auditPage_ < pages);
}

void DashboardWidget::setAuditLogs(const QVector<AuditLogRow>& logs, int total, int page, int pageSize) {
    if (!auditTable_) return;
    auditTable_->setRowCount(0);
    for (const auto& log : logs) {
        const int row = auditTable_->rowCount();
        auditTable_->insertRow(row);
        auditTable_->setItem(row, 0, new QTableWidgetItem(log.time));
        auditTable_->setItem(row, 1, new QTableWidgetItem(log.actor));
        auditTable_->setItem(row, 2, new QTableWidgetItem(log.action));
        auditTable_->setItem(row, 3, new QTableWidgetItem(log.target.isEmpty() ? QStringLiteral("-") : log.target));
        auditTable_->setItem(row, 4, new QTableWidgetItem(log.result));
        auditTable_->setItem(row, 5, new QTableWidgetItem(log.actorIp.isEmpty() ? QStringLiteral("-") : log.actorIp));
        if (!log.detail.isEmpty()) auditTable_->item(row, 2)->setToolTip(log.detail);
    }
    applyAuditPagination(total, page, pageSize);
}
