#include "ClientSession.h"

#include <QDateTime>

#include <cassert>
#include <iostream>

int main() {
    ClientSession session;
    const auto baseTime = QDateTime::currentMSecsSinceEpoch();
    const auto requestId = session.begin(QStringLiteral("login"));
    assert(session.contains(requestId));
    assert(session.expire(baseTime + 4999, 5000).isEmpty());

    const auto expired = session.expire(baseTime + 5001, 5000);
    assert(expired.size() == 1);
    assert(expired.front().first == requestId);
    assert(expired.front().second == QStringLiteral("login"));
    assert(!session.contains(requestId));

    std::cout << "test_client_session passed\n";
    return 0;
}
