#include "DashboardWidget.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    DashboardWidget dashboard;

    auto* navigation = dashboard.findChild<QListWidget*>(QStringLiteral("dashboardNavigation"));
    auto* pages = dashboard.findChild<QStackedWidget*>(QStringLiteral("dashboardPages"));
    auto* usersTable = dashboard.findChild<QTableWidget*>(QStringLiteral("usersTable"));
    auto* resetButton = dashboard.findChild<QPushButton*>(QStringLiteral("resetPasswordButton"));
    auto* freezeButton = dashboard.findChild<QPushButton*>(QStringLiteral("freezeUserButton"));
    auto* accountButton = dashboard.findChild<QPushButton*>(QStringLiteral("accountBar"));
    const auto* forceOfflineButton = dashboard.findChild<QPushButton*>(QStringLiteral("forceOfflineButton"));
    auto* onlineTable = dashboard.findChild<QTableWidget*>(QStringLiteral("onlineTable"));
    auto* auditTable = dashboard.findChild<QTableWidget*>(QStringLiteral("auditTable"));
    auto* overviewTable = dashboard.findChild<QTableWidget*>(QStringLiteral("overviewActivityTable"));
    auto* myActivityTable = dashboard.findChild<QTableWidget*>(QStringLiteral("myActivityTable"));
    auto* searchEdit = dashboard.findChild<QLineEdit*>(QStringLiteral("userSearchEdit"));
    auto* searchButton = dashboard.findChild<QPushButton*>(QStringLiteral("searchUsersButton"));
    auto* previousPage = dashboard.findChild<QPushButton*>(QStringLiteral("previousPageButton"));
    auto* nextPage = dashboard.findChild<QPushButton*>(QStringLiteral("nextPageButton"));
    auto* userPageLabel = dashboard.findChild<QLabel*>(QStringLiteral("userPageLabel"));
    auto* statusFilter = dashboard.findChild<QComboBox*>(QStringLiteral("userStatusFilter"));
    auto* batchToggle = dashboard.findChild<QPushButton*>(QStringLiteral("batchToggleButton"));
    auto* exportButton = dashboard.findChild<QPushButton*>(QStringLiteral("exportUsersButton"));
    auto* deleteButton = dashboard.findChild<QPushButton*>(QStringLiteral("deleteUserButton"));

    // 导航：数据总览 / 用户管理 / 在线用户管理 / 审计日志 / 服务端监控 / 我的空间，
    // 其中「我的空间」与头像点击共用同一页（index 5）。
    assert(navigation && navigation->count() == 6);
    assert(pages && pages->count() == 6);
    assert(usersTable && usersTable->columnCount() == 4);
    assert(resetButton && !resetButton->isEnabled());
    assert(freezeButton && !freezeButton->isEnabled());
    // 批量删除按钮已按需求移除（不允许随手删除用户账号）；
    // 但保留单条「删除账号」入口，未选中记录时必须禁用。
    assert(batchToggle && exportButton);
    assert(dashboard.findChild<QPushButton*>(QStringLiteral("batchDeleteButton")) == nullptr);
    assert(deleteButton && !deleteButton->isEnabled());
    assert(accountButton);
    assert(forceOfflineButton && !forceOfflineButton->isEnabled());
    assert(onlineTable && onlineTable->columnCount() == 5);
    assert(auditTable && auditTable->columnCount() == 6);
    assert(overviewTable && overviewTable->columnCount() == 5);
    assert(myActivityTable && myActivityTable->columnCount() == 4);
    assert(searchButton && searchEdit);
    assert(statusFilter && statusFilter->count() == 3);
    assert(previousPage && !previousPage->isEnabled());
    assert(nextPage);

    // 数据总览统计卡片。
    dashboard.setOverviewStats(20, 17, 3, 2);
    auto* statTotal = dashboard.findChild<QLabel*>(QStringLiteral("statValueTotal"));
    auto* statOnline = dashboard.findChild<QLabel*>(QStringLiteral("statValueOnline"));
    assert(statTotal && statTotal->text() == QStringLiteral("20"));
    assert(statOnline && statOnline->text() == QStringLiteral("2"));

    // 服务端监控指标。
    dashboard.setServerStats({3600, 5, 42, 4, 4, QStringLiteral("memory"), 3, 12});
    auto* uptime = dashboard.findChild<QLabel*>(QStringLiteral("monitorValue"));
    // monitorValue 是第一个创建的监控标签（运行时长）。
    assert(uptime && uptime->text() == QStringLiteral("1小时 0分 0秒"));

    navigation->setCurrentRow(1);
    assert(pages->currentIndex() == 1);

    // 分页：25 条记录、每页 10 条 → 3 页。
    dashboard.setUsers({{7, QStringLiteral("alice"), QStringLiteral("admin"), QStringLiteral("active")}}, 25, 2, 10);
    assert(usersTable->currentRow() == 0);
    assert(resetButton->isEnabled());
    assert(freezeButton->isEnabled());
    assert(previousPage->isEnabled());
    assert(nextPage->isEnabled());
    assert(userPageLabel->text() == QStringLiteral("第 2 / 3 页"));

    // 最后一页时“下一页”应禁用、“上一页”可用。
    dashboard.setUsers({}, 25, 3, 10);
    assert(!nextPage->isEnabled());
    assert(previousPage->isEnabled());

    dashboard.setCurrentUser(QStringLiteral("alice"), QStringLiteral("admin"));
    assert(dashboard.findChild<QLabel*>(QStringLiteral("accountName"))->text() == QStringLiteral("alice"));

    // 点击头像进入个人中心（index 5）。
    accountButton->click();
    assert(pages->currentIndex() == 5);
    assert(dashboard.findChild<QLabel*>(QStringLiteral("profileName"))->text() == QStringLiteral("alice"));

    // 普通用户：管理员页面全部隐藏，仅保留「我的空间」，并自动切到该页。
    dashboard.setCurrentUser(QStringLiteral("bob"), QStringLiteral("user"));
    assert(navigation->item(0)->isHidden());
    assert(navigation->item(1)->isHidden());
    assert(navigation->item(2)->isHidden());
    assert(navigation->item(3)->isHidden());
    assert(navigation->item(4)->isHidden());
    assert(!navigation->item(5)->isHidden());
    assert(pages->currentIndex() == 5);

    // WHOAMI 账号信息卡片。
    dashboard.setAccountInfo({7, QStringLiteral("bob"), QStringLiteral("user"), QStringLiteral("active"),
                              QStringLiteral("127.0.0.1:4444"), 1762900000, 1762900100, 1762900110});
    auto* userIdLabel = dashboard.findChild<QLabel*>(QStringLiteral("profileUserId"));
    auto* statusLabel = dashboard.findChild<QLabel*>(QStringLiteral("profileStatus"));
    assert(userIdLabel && userIdLabel->text() == QStringLiteral("7"));
    assert(statusLabel && statusLabel->text() == QStringLiteral("正常"));

    dashboard.setOnlineSessions({{QStringLiteral("12"), QStringLiteral("alice"), QStringLiteral("admin"), true, QStringLiteral("127.0.0.1:4444")}});
    assert(onlineTable->rowCount() == 1);
    assert(onlineTable->item(0, 1)->text() == QStringLiteral("alice"));
    assert(onlineTable->item(0, 4)->text() == QStringLiteral("127.0.0.1:4444"));

    dashboard.setAuditLogs({{9, QStringLiteral("2026-11-10 12:00:00"), QStringLiteral("alice"),
                             QStringLiteral("auth.login"), QString{}, QStringLiteral("success"), QString{}, QStringLiteral("127.0.0.1")}}, 42, 1, 10);
    assert(auditTable->rowCount() == 1);
    assert(auditTable->item(0, 1)->text() == QStringLiteral("alice"));
    assert(auditTable->item(0, 3)->text() == QStringLiteral("-"));

    dashboard.setOverviewActivity({{9, QStringLiteral("2026-11-10 12:00:00"), QStringLiteral("alice"),
                                    QStringLiteral("auth.login"), QString{}, QStringLiteral("success"), QString{}, QStringLiteral("127.0.0.1")}});
    assert(overviewTable->rowCount() == 1);

    dashboard.setMyActivity({{10, QStringLiteral("2026-11-10 12:30:00"), QStringLiteral("alice"),
                              QStringLiteral("user.change_password"), QStringLiteral("alice"), QStringLiteral("success"), QString{}, QStringLiteral("127.0.0.1")}});
    assert(myActivityTable->rowCount() == 1);
    assert(myActivityTable->item(0, 1)->text() == QStringLiteral("user.change_password"));

    std::cout << "test_dashboard_widget passed\n";
    return 0;
}
