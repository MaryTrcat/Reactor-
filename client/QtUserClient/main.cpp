#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Reactor User Workbench"));
    app.setOrganizationName(QStringLiteral("ReactorUserService"));

    QCommandLineParser parser;
    parser.addHelpOption();
    QCommandLineOption usernameOption(QStringList{QStringLiteral("u"), QStringLiteral("username")}, QStringLiteral("自动登录账号"), QStringLiteral("username"));
    QCommandLineOption passwordOption(QStringList{QStringLiteral("p"), QStringLiteral("password")}, QStringLiteral("自动登录密码"), QStringLiteral("password"));
    QCommandLineOption autoLoginOption(QStringLiteral("auto-login"), QStringLiteral("启动后自动登录"));
    parser.addOption(usernameOption);
    parser.addOption(passwordOption);
    parser.addOption(autoLoginOption);
    parser.process(app);

    MainWindow window;
    window.show();
    if (parser.isSet(autoLoginOption) && parser.isSet(usernameOption) && parser.isSet(passwordOption)) {
        const auto username = parser.value(usernameOption);
        const auto password = parser.value(passwordOption);
        QTimer::singleShot(0, &window, [&window, username, password]() {
            window.autoLogin(username, password);
        });
    }
    return app.exec();
}
