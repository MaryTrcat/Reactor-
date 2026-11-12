#include "RegisterWidget.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    RegisterWidget widget;

    auto* username = widget.findChild<QLineEdit*>(QStringLiteral("registerUsername"));
    auto* password = widget.findChild<QLineEdit*>(QStringLiteral("registerPassword"));
    auto* confirm = widget.findChild<QLineEdit*>(QStringLiteral("registerConfirm"));
    auto* button = widget.findChild<QPushButton*>(QStringLiteral("registerButton"));

    assert(username);
    assert(password);
    assert(confirm);
    assert(password->echoMode() == QLineEdit::Password);
    assert(confirm->echoMode() == QLineEdit::Password);
    assert(button);
    assert(username->minimumHeight() >= 42);
    assert(password->minimumHeight() >= 42);
    assert(confirm->minimumHeight() >= 42);
    auto* status = widget.findChild<QLabel*>(QStringLiteral("statusLabel"));
    assert(status);
    assert(status->text().isEmpty());
    assert(status->isHidden());

    // 两次密码不一致：本地拦截，不发请求。
    bool emitted = false;
    QObject::connect(&widget, &RegisterWidget::registerRequested,
                     [&emitted](const QString&, const QString&, const QString&) { emitted = true; });
    username->setText(QStringLiteral("alice"));
    password->setText(QStringLiteral("123456"));
    confirm->setText(QStringLiteral("654321"));
    button->click();
    assert(!emitted);
    assert(!status->isHidden());
    assert(status->text() == QStringLiteral("两次输入的密码不一致"));

    // 密码过短：本地拦截。
    password->setText(QStringLiteral("123"));
    confirm->setText(QStringLiteral("123"));
    button->click();
    assert(!emitted);
    assert(status->text() == QStringLiteral("密码至少需要 6 位"));

    // 用户名非法字符：本地拦截。
    password->setText(QStringLiteral("123456"));
    confirm->setText(QStringLiteral("123456"));
    username->setText(QStringLiteral("bad name!"));
    button->click();
    assert(!emitted);
    assert(status->text() == QStringLiteral("用户名只能包含字母、数字、下划线或连字符"));

    // 合法输入：发出注册信号。
    username->setText(QStringLiteral("alice-01"));
    button->click();
    assert(emitted);

    // 回车提交：确认密码框内按 Enter 也触发注册。
    emitted = false;
    confirm->setFocus();
    confirm->returnPressed();
    assert(emitted);

    widget.setStatus(QStringLiteral("注册成功"), false);
    assert(!status->isHidden());
    assert(status->text() == QStringLiteral("注册成功"));

    std::cout << "test_register_widget passed\n";
    return 0;
}
