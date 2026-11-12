#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;

// 自助注册页：未登录用户直接创建普通账号（角色由服务端强制为 user）。
class RegisterWidget : public QWidget {
    Q_OBJECT

public:
    explicit RegisterWidget(QWidget* parent = nullptr);
    void setStatus(const QString& message, bool error = false);

signals:
    // 用户名/密码/确认密码已经过本地预校验。
    void registerRequested(const QString& username, const QString& password, const QString& confirmPassword);
    void backRequested();

private:
    // 本地预校验：通过后发出 registerRequested。
    void trySubmit();

    QLineEdit* usernameEdit_{nullptr};
    QLineEdit* passwordEdit_{nullptr};
    QLineEdit* confirmEdit_{nullptr};
    QLabel* statusLabel_{nullptr};
};
