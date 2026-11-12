#include "ClientSession.h"

#include <QDateTime>

quint32 ClientSession::begin(const QString& operation) {
    const quint32 requestId = nextRequestId_++;
    pending_.insert(requestId, operation);
    startedAtMs_.insert(requestId, QDateTime::currentMSecsSinceEpoch());
    return requestId;
}

QString ClientSession::take(quint32 requestId) {
    const auto it = pending_.find(requestId);
    if (it == pending_.end()) {
        return {};
    }
    const QString operation = it.value();
    pending_.erase(it);
    startedAtMs_.remove(requestId);
    return operation;
}

QList<QPair<quint32, QString>> ClientSession::expire(qint64 nowMs, qint64 timeoutMs) {
    QList<QPair<quint32, QString>> expired;
    for (auto it = startedAtMs_.cbegin(); it != startedAtMs_.cend();) {
        if (nowMs - it.value() >= timeoutMs) {
            const quint32 requestId = it.key();
            expired.append({requestId, pending_.value(requestId)});
            pending_.remove(requestId);
            it = startedAtMs_.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

bool ClientSession::contains(quint32 requestId) const {
    return pending_.contains(requestId);
}
