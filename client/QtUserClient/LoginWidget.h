#pragma once

#include <QWidget>

class QLineEdit;
class QLabel;

class LoginWidget : public QWidget {
    Q_OBJECT

public:
    explicit LoginWidget(QWidget* parent = nullptr);
    void setStatus(const QString& message, bool error = false);
    void setUsername(const QString& username);

signals:
    void loginRequested(const QString& username, const QString& password);
    // 点击「立即注册」：主窗口切换到注册页。
    void registerPageRequested();

private:
    QLineEdit* usernameEdit_{nullptr};
    QLineEdit* passwordEdit_{nullptr};
    QLabel* statusLabel_{nullptr};
};
