#pragma once

#include <QHash>
#include <QPair>
#include <QString>
#include <QList>

class ClientSession {
public:
    quint32 begin(const QString& operation);
    QString take(quint32 requestId);
    QList<QPair<quint32, QString>> expire(qint64 nowMs, qint64 timeoutMs);
    bool contains(quint32 requestId) const;

private:
    quint32 nextRequestId_{1};
    QHash<quint32, QString> pending_;
    QHash<quint32, qint64> startedAtMs_;
};
