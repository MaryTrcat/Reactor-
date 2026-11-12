#include "LoginWidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

LoginWidget::LoginWidget(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 16, 32, 16);
    outer->setSpacing(12);

    // 上下各一个拉伸：窗口略高于内容时表单整体垂直居中。
    outer->addStretch(1);

    // 标题固定高度，避免布局把多余空间分给标题导致标题和输入框之间出现大空隙。
    auto* title = new QLabel(QStringLiteral("用户登录"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outer->addWidget(title);
    outer->addSpacing(4);

    // 无外框设计：输入框与按钮统一 250px 宽并居中。
    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setObjectName(QStringLiteral("loginUsername"));
    usernameEdit_->setPlaceholderText(QStringLiteral("用户名"));
    usernameEdit_->setMinimumHeight(44);
    usernameEdit_->setFixedWidth(250);
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setObjectName(QStringLiteral("loginPassword"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("密码"));
    passwordEdit_->setMinimumHeight(44);
    passwordEdit_->setFixedWidth(250);
    outer->addWidget(usernameEdit_, 0, Qt::AlignHCenter);
    outer->addWidget(passwordEdit_, 0, Qt::AlignHCenter);
    outer->addSpacing(4);

    auto* button = new QPushButton(QStringLiteral("登录"), this);
    button->setObjectName(QStringLiteral("loginButton"));
    button->setProperty("primary", true);
    button->setMinimumHeight(46);
    button->setFixedWidth(250);
    button->setDefault(true);
    outer->addWidget(button, 0, Qt::AlignHCenter);

    // 注册入口：扁平链接样式，点击切换到注册页。
    auto* registerLink = new QPushButton(QStringLiteral("没有账号？立即注册"), this);
    registerLink->setObjectName(QStringLiteral("registerLink"));
    registerLink->setFlat(true);
    registerLink->setFixedWidth(250);
    registerLink->setCursor(Qt::PointingHandCursor);
    outer->addWidget(registerLink, 0, Qt::AlignHCenter);
    outer->addStretch(1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setVisible(false);
    statusLabel_->setFixedWidth(250);
    statusLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outer->addWidget(statusLabel_, 0, Qt::AlignHCenter);

    setTabOrder(usernameEdit_, passwordEdit_);
    setTabOrder(passwordEdit_, button);
    setTabOrder(button, registerLink);

    connect(button, &QPushButton::clicked, this, [this]() {
        emit loginRequested(usernameEdit_->text(), passwordEdit_->text());
    });
    // 回车提交：两个输入框内按 Enter 都直接触发登录。
    const auto submit = [this]() {
        emit loginRequested(usernameEdit_->text(), passwordEdit_->text());
    };
    connect(usernameEdit_, &QLineEdit::returnPressed, this, submit);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, submit);
    connect(registerLink, &QPushButton::clicked, this, &LoginWidget::registerPageRequested);
}

void LoginWidget::setStatus(const QString& message, bool error) {
    statusLabel_->setText(message);
    statusLabel_->setVisible(!message.isEmpty());
    statusLabel_->setProperty("error", error);
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}

void LoginWidget::setUsername(const QString& username) {
    usernameEdit_->setText(username);
    passwordEdit_->clear();
}
