#include "QtTcpClient.h"

#include <QAbstractSocket>

#include <cstdint>
#include <exception>

QtTcpClient::QtTcpClient(QObject* parent) : QObject(parent) {}

void QtTcpClient::connectToServer(const QString& host, quint16 port) {
    if (socket_) {
        socket_->abort();
        socket_->deleteLater();
        socket_ = nullptr;
    }

    codec_ = protocol::Codec{};
    socket_ = new QTcpSocket(this);
    connect(socket_, &QTcpSocket::connected, this, &QtTcpClient::connected);
    connect(socket_, &QTcpSocket::disconnected, this, &QtTcpClient::disconnected);
    connect(socket_, &QTcpSocket::readyRead, this, &QtTcpClient::onReadyRead);
    connect(socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        if (socket_) {
            emit errorOccurred(socket_->errorString());
        }
    });
    socket_->connectToHost(host, port);
}

void QtTcpClient::disconnectFromServer() {
    if (socket_) {
        socket_->disconnectFromHost();
    }
}

void QtTcpClient::sendFrame(quint16 messageType, quint32 requestId, const QByteArray& body) {
    if (!socket_ || socket_->state() != QAbstractSocket::ConnectedState) {
        emit errorOccurred(QStringLiteral("服务端未连接"));
        return;
    }

    protocol::Frame frame{messageType, requestId, std::string(body.constData(), static_cast<std::size_t>(body.size()))};
    const auto bytes = codec_.encode(frame);
    const auto written = socket_->write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    if (written != static_cast<qint64>(bytes.size())) {
        emit errorOccurred(QStringLiteral("请求发送不完整"));
    }
}

void QtTcpClient::onReadyRead() {
    if (!socket_) {
        return;
    }

    const QByteArray bytes = socket_->readAll();
    codec_.append(reinterpret_cast<const std::uint8_t*>(bytes.constData()), static_cast<std::size_t>(bytes.size()));
    try {
        while (auto frame = codec_.tryDecode()) {
            emit frameReceived(frame->message_type,
                               frame->request_id,
                               QByteArray(frame->body.data(), static_cast<int>(frame->body.size())));
        }
    } catch (const std::exception& ex) {
        emit errorOccurred(QString::fromUtf8(ex.what()));
    }
}
