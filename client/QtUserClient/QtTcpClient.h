#pragma once

#include "protocol/Codec.h"

#include <QObject>
#include <QTcpSocket>

class QtTcpClient : public QObject {
    Q_OBJECT

public:
    explicit QtTcpClient(QObject* parent = nullptr);

public slots:
    void connectToServer(const QString& host, quint16 port);
    void disconnectFromServer();
    void sendFrame(quint16 messageType, quint32 requestId, const QByteArray& body);

signals:
    void connected();
    void disconnected();
    void frameReceived(quint16 messageType, quint32 requestId, const QByteArray& body);
    void errorOccurred(const QString& message);

private slots:
    void onReadyRead();

private:
    QTcpSocket* socket_{nullptr};
    protocol::Codec codec_;
};
