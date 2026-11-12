#include "LoginWidget.h"

#include <QApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    LoginWidget login;

    auto* username = login.findChild<QLineEdit*>(QStringLiteral("loginUsername"));
    auto* password = login.findChild<QLineEdit*>(QStringLiteral("loginPassword"));
    auto* loginButton = login.findChild<QPushButton*>(QStringLiteral("loginButton"));

    assert(username);
    assert(password);
    assert(password->echoMode() == QLineEdit::Password);
    assert(loginButton);
    assert(username->minimumHeight() >= 42);
    assert(password->minimumHeight() >= 42);
    assert(loginButton->minimumHeight() >= 44);
    auto* status = login.findChild<QLabel*>(QStringLiteral("statusLabel"));
    assert(status);
    assert(status->text().isEmpty());
    assert(status->isHidden());

    login.setStatus(QStringLiteral("登录失败"), true);
    assert(!status->isHidden());
    assert(status->text() == QStringLiteral("登录失败"));
    assert(login.findChild<QSpinBox*>() == nullptr);
    assert(login.findChild<QLineEdit*>(QStringLiteral("loginHost")) == nullptr);
    assert(login.findChild<QPushButton*>(QStringLiteral("bootstrapRegisterButton")) == nullptr);

    // 回车提交：密码框内按 Enter 直接触发登录信号。
    bool loginEmitted = false;
    QObject::connect(&login, &LoginWidget::loginRequested,
                     [&loginEmitted](const QString&, const QString&) { loginEmitted = true; });
    username->setText(QStringLiteral("admin"));
    password->setText(QStringLiteral("123456"));
    password->returnPressed();
    assert(loginEmitted);

    std::cout << "test_login_widget passed\n";
    return 0;
}
