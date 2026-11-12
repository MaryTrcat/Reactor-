#include "RegisterWidget.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>
#include <QVBoxLayout>

RegisterWidget::RegisterWidget(QWidget* parent) : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(32, 16, 32, 16);
    outer->setSpacing(12);

    // 与登录页一致：上下各一个拉伸，窗口略高于内容时表单整体垂直居中。
    outer->addStretch(1);

    auto* title = new QLabel(QStringLiteral("注册新账号"), this);
    title->setObjectName(QStringLiteral("pageTitle"));
    title->setAlignment(Qt::AlignCenter);
    title->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outer->addWidget(title);
    outer->addSpacing(4);

    // 与登录页一致的 250px 紧凑表单。
    usernameEdit_ = new QLineEdit(this);
    usernameEdit_->setObjectName(QStringLiteral("registerUsername"));
    usernameEdit_->setPlaceholderText(QStringLiteral("用户名（3-32 位字母/数字/_/-）"));
    usernameEdit_->setMinimumHeight(44);
    usernameEdit_->setFixedWidth(250);
    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setObjectName(QStringLiteral("registerPassword"));
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(QStringLiteral("密码（至少 6 位）"));
    passwordEdit_->setMinimumHeight(44);
    passwordEdit_->setFixedWidth(250);
    confirmEdit_ = new QLineEdit(this);
    confirmEdit_->setObjectName(QStringLiteral("registerConfirm"));
    confirmEdit_->setEchoMode(QLineEdit::Password);
    confirmEdit_->setPlaceholderText(QStringLiteral("确认密码"));
    confirmEdit_->setMinimumHeight(44);
    confirmEdit_->setFixedWidth(250);
    outer->addWidget(usernameEdit_, 0, Qt::AlignHCenter);
    outer->addWidget(passwordEdit_, 0, Qt::AlignHCenter);
    outer->addWidget(confirmEdit_, 0, Qt::AlignHCenter);
    outer->addSpacing(4);

    auto* button = new QPushButton(QStringLiteral("注册"), this);
    button->setObjectName(QStringLiteral("registerButton"));
    button->setProperty("primary", true);
    button->setMinimumHeight(46);
    button->setFixedWidth(250);
    button->setDefault(true);
    outer->addWidget(button, 0, Qt::AlignHCenter);

    auto* backLink = new QPushButton(QStringLiteral("已有账号？返回登录"), this);
    backLink->setObjectName(QStringLiteral("backToLoginLink"));
    backLink->setFlat(true);
    backLink->setFixedWidth(250);
    backLink->setCursor(Qt::PointingHandCursor);
    outer->addWidget(backLink, 0, Qt::AlignHCenter);
    outer->addStretch(1);

    statusLabel_ = new QLabel(this);
    statusLabel_->setObjectName(QStringLiteral("statusLabel"));
    statusLabel_->setAlignment(Qt::AlignCenter);
    statusLabel_->setVisible(false);
    statusLabel_->setFixedWidth(250);
    statusLabel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    outer->addWidget(statusLabel_, 0, Qt::AlignHCenter);

    setTabOrder(usernameEdit_, passwordEdit_);
    setTabOrder(passwordEdit_, confirmEdit_);
    setTabOrder(confirmEdit_, button);
    setTabOrder(button, backLink);

    connect(button, &QPushButton::clicked, this, &RegisterWidget::trySubmit);
    // 回车提交：三个输入框内按 Enter 都直接触发注册。
    connect(usernameEdit_, &QLineEdit::returnPressed, this, &RegisterWidget::trySubmit);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &RegisterWidget::trySubmit);
    connect(confirmEdit_, &QLineEdit::returnPressed, this, &RegisterWidget::trySubmit);
    connect(backLink, &QPushButton::clicked, this, &RegisterWidget::backRequested);
}

void RegisterWidget::trySubmit() {
    // 本地预校验与登录页同样的规则，先给用户即时反馈再发请求。
    const auto username = usernameEdit_->text().trimmed();
    const auto password = passwordEdit_->text();
    const auto confirm = confirmEdit_->text();
    if (username.size() < 3 || username.size() > 32) {
        setStatus(QStringLiteral("用户名长度需要 3-32 个字符"), true);
        return;
    }
    for (const QChar& ch : username) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('_') && ch != QLatin1Char('-')) {
            setStatus(QStringLiteral("用户名只能包含字母、数字、下划线或连字符"), true);
            return;
        }
    }
    if (password.size() < 6) {
        setStatus(QStringLiteral("密码至少需要 6 位"), true);
        return;
    }
    if (password != confirm) {
        setStatus(QStringLiteral("两次输入的密码不一致"), true);
        return;
    }
    emit registerRequested(username, password, confirm);
}

void RegisterWidget::setStatus(const QString& message, bool error) {
    statusLabel_->setText(message);
    statusLabel_->setVisible(!message.isEmpty());
    statusLabel_->setProperty("error", error);
    statusLabel_->style()->unpolish(statusLabel_);
    statusLabel_->style()->polish(statusLabel_);
}
