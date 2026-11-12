#include "ProtoMessages.h"

#include <QCoreApplication>

#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    // ===== 既有行为回归 =====
    const auto login = qtproto::loginRequest(QStringLiteral("alice"), QStringLiteral("secret1"));
    assert(login.toHex() == QByteArrayLiteral("0a05616c696365120773656372657431"));

    qtproto::LoginResponse loginResponse;
    const QByteArray encodedLoginResponse = QByteArray::fromHex(QByteArrayLiteral("080110071a0561646d696e22086c6f67696e206f6b"));
    assert(qtproto::decodeLoginResponse(encodedLoginResponse, loginResponse));
    assert(loginResponse.success);
    assert(loginResponse.userId == 7);
    assert(loginResponse.role == QStringLiteral("admin"));
    assert(loginResponse.remainingAttempts == -1);

    qtproto::ListUsersResponse users;
    const QByteArray encodedUsers = QByteArray::fromHex(QByteArrayLiteral("0801121008011205616c6963651a0561646d696e1a0c7573657273206c6973746564"));
    assert(qtproto::decodeListUsersResponse(encodedUsers, users));
    assert(users.success);
    assert(users.users.size() == 1);
    assert(users.users.front().username == QStringLiteral("alice"));
    assert(users.users.front().role == QStringLiteral("admin"));

    // ===== 登录失败 + 剩余次数（字段 6）=====
    qtproto::LoginResponse limited;
    // success=false, message="invalid", remaining_attempts=3
    assert(qtproto::decodeLoginResponse(QByteArray::fromHex(QByteArrayLiteral("08002207696e76616c69643003")), limited));
    assert(!limited.success);
    assert(limited.message == QStringLiteral("invalid"));
    assert(limited.remainingAttempts == 3);

    // ===== 分页请求：page=2, page_size=10, keyword="li", status="frozen" =====
    const auto paged = qtproto::listUsersRequest(2, 10, QStringLiteral("li"), QStringLiteral("frozen"));
    assert(paged.toHex() == QByteArrayLiteral("0802100a1a026c69220666726f7a656e"));

    // ===== 分页响应：success=1, total=25, page=3, page_size=10 =====
    qtproto::ListUsersResponse pagedResponse;
    assert(qtproto::decodeListUsersResponse(QByteArray::fromHex(QByteArrayLiteral("080120192803300a")), pagedResponse));
    assert(pagedResponse.success);
    assert(pagedResponse.total == 25);
    assert(pagedResponse.page == 3);
    assert(pagedResponse.pageSize == 10);

    // ===== 改密请求带旧密码：username=1, new_password=2, old_password=3 =====
    const auto change = qtproto::changePasswordRequest(QStringLiteral("alice"), QStringLiteral("old123"), QStringLiteral("new456"));
    assert(change.toHex() == QByteArrayLiteral("0a05616c69636512066e65773435361a066f6c64313233"));

    // ===== 重置密码响应：success=1, message="ok", generated_password="x19f3abd32c" =====
    qtproto::ResetPasswordResponse reset;
    assert(qtproto::decodeResetPasswordResponse(QByteArray::fromHex(QByteArrayLiteral("080112026f6b1a0b7831396633616264333263")), reset));
    assert(reset.success);
    assert(reset.message == QStringLiteral("ok"));
    assert(reset.generatedPassword == QStringLiteral("x19f3abd32c"));

    // ===== PONG：client_time_ms=1, server_time_ms=999 =====
    qtproto::PongInfo pong;
    assert(qtproto::decodePongResponse(QByteArray::fromHex(QByteArrayLiteral("080110e707")), pong));
    assert(pong.clientTimeMs == 1);
    assert(pong.serverTimeMs == 999);

    // ===== 审计请求编码：page / page_size / actor =====
    assert(qtproto::listAuditLogsRequest(1, 10, QString{}).toHex() == QByteArrayLiteral("0801100a"));
    assert(qtproto::listAuditLogsRequest(2, 10, QStringLiteral("alice")).toHex() == QByteArrayLiteral("0802100a1a05616c696365"));

    // ===== 审计响应解码：1 条 entry（id/actor/action/result/actor_ip/created_at）+ 分页 =====
    // entry = id=9, actor="alice", action="auth.login", result="success", actor_ip="127.0.0.1", created_at=1000
    qtproto::ListAuditLogsResponse audit;
    const QByteArray auditBytes = QByteArray::fromHex(QByteArrayLiteral(
        "0801"
        "122c"
        "08091205616c6963651a0a617574682e6c6f67696e2a07737563636573733a093132372e302e302e3140e807"
        "202a2801300a"));
    assert(qtproto::decodeListAuditLogsResponse(auditBytes, audit));
    assert(audit.success);
    assert(audit.total == 42);
    assert(audit.page == 1);
    assert(audit.pageSize == 10);
    assert(audit.entries.size() == 1);
    const auto& entry = audit.entries.front();
    assert(entry.id == 9);
    assert(entry.actor == QStringLiteral("alice"));
    assert(entry.action == QStringLiteral("auth.login"));
    assert(entry.result == QStringLiteral("success"));
    assert(entry.actorIp == QStringLiteral("127.0.0.1"));
    assert(entry.createdAt == 1000);

    // ===== 在线会话解码：新增 peer_address（字段 5）=====
    qtproto::ListOnlineResponse online;
    // session: session_id="12", username="alice", role="admin", authenticated=true, peer_address="127.0.0.1:4444"
    assert(qtproto::decodeListOnlineResponse(QByteArray::fromHex(QByteArrayLiteral(
        "0801"
        "1224"
        "0a0231321205616c6963651a0561646d696e2001"
        "2a0e3132372e302e302e313a34343434"
        "1a046f6b6179")), online));
    assert(online.success);
    assert(online.sessions.size() == 1);
    assert(online.sessions.front().peerAddress == QStringLiteral("127.0.0.1:4444"));

    std::cout << "test_qt_proto_messages passed\n";
    return 0;
}
