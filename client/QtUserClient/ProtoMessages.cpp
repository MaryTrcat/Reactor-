#include "ProtoMessages.h"

#include <cstdint>

namespace qtproto {
namespace {

void appendVarint(QByteArray& output, std::uint64_t value) {
    while (value >= 0x80) {
        output.append(static_cast<char>((value & 0x7f) | 0x80));
        value >>= 7;
    }
    output.append(static_cast<char>(value));
}

void appendKey(QByteArray& output, int field, int wireType) {
    appendVarint(output, static_cast<std::uint64_t>((field << 3) | wireType));
}

void appendString(QByteArray& output, int field, const QString& value) {
    const auto bytes = value.toUtf8();
    appendKey(output, field, 2);
    appendVarint(output, static_cast<std::uint64_t>(bytes.size()));
    output.append(bytes);
}

void appendBool(QByteArray& output, int field, bool value) {
    appendKey(output, field, 0);
    appendVarint(output, value ? 1 : 0);
}

void appendInt64(QByteArray& output, int field, qint64 value) {
    appendKey(output, field, 0);
    appendVarint(output, static_cast<std::uint64_t>(value));
}

bool readVarint(const QByteArray& input, int& offset, std::uint64_t& value) {
    value = 0;
    int shift = 0;
    while (offset < input.size() && shift < 64) {
        const auto byte = static_cast<std::uint8_t>(input.at(offset++));
        value |= static_cast<std::uint64_t>(byte & 0x7f) << shift;
        if ((byte & 0x80) == 0) {
            return true;
        }
        shift += 7;
    }
    return false;
}

bool readBytes(const QByteArray& input, int& offset, QByteArray& value) {
    std::uint64_t length = 0;
    if (!readVarint(input, offset, length) || length > static_cast<std::uint64_t>(input.size() - offset)) {
        return false;
    }
    value = input.mid(offset, static_cast<int>(length));
    offset += static_cast<int>(length);
    return true;
}

bool skipField(const QByteArray& input, int& offset, int wireType) {
    if (wireType == 0) {
        std::uint64_t ignored = 0;
        return readVarint(input, offset, ignored);
    }
    if (wireType == 1) {
        offset += 8;
        return offset <= input.size();
    }
    if (wireType == 2) {
        QByteArray ignored;
        return readBytes(input, offset, ignored);
    }
    if (wireType == 5) {
        offset += 4;
        return offset <= input.size();
    }
    return false;
}

template <typename Handler>
bool parseMessage(const QByteArray& input, Handler handler) {
    int offset = 0;
    while (offset < input.size()) {
        std::uint64_t key = 0;
        if (!readVarint(input, offset, key)) {
            return false;
        }
        const int field = static_cast<int>(key >> 3);
        const int wireType = static_cast<int>(key & 0x07);
        if (!handler(field, wireType, input, offset)) {
            return false;
        }
    }
    return true;
}

QByteArray twoStrings(int firstField, const QString& first, int secondField, const QString& second) {
    QByteArray output;
    appendString(output, firstField, first);
    appendString(output, secondField, second);
    return output;
}

}  // namespace

QByteArray registerRequest(const QString& username, const QString& password, const QString& role) {
    QByteArray output = twoStrings(1, username, 2, password);
    appendString(output, 3, role);
    return output;
}

QByteArray loginRequest(const QString& username, const QString& password) {
    return twoStrings(1, username, 2, password);
}

QByteArray queryUserRequest(const QString& username) {
    QByteArray output;
    appendString(output, 1, username);
    return output;
}

QByteArray checkPermissionRequest(const QString& username, const QString& role) {
    return twoStrings(1, username, 2, role);
}

QByteArray changePasswordRequest(const QString& username, const QString& oldPassword, const QString& newPassword) {
    QByteArray output = twoStrings(1, username, 2, newPassword);
    appendString(output, 3, oldPassword);
    return output;
}

QByteArray deleteUserRequest(const QString& username) {
    return queryUserRequest(username);
}

QByteArray listUsersRequest(int page, int pageSize, const QString& keyword, const QString& status) {
    QByteArray output;
    appendInt64(output, 1, page);
    appendInt64(output, 2, pageSize);
    if (!keyword.isEmpty()) appendString(output, 3, keyword);
    if (!status.isEmpty()) appendString(output, 4, status);
    return output;
}

QByteArray resetPasswordRequest(const QString& username) {
    return queryUserRequest(username);
}

QByteArray setUserStatusRequest(const QString& username, bool frozen) {
    QByteArray output = queryUserRequest(username);
    appendBool(output, 2, frozen);
    return output;
}

QByteArray listOnlineRequest() {
    return {};
}

QByteArray forceLogoutRequest(const QString& sessionId) {
    QByteArray output;
    appendString(output, 1, sessionId);
    return output;
}

QByteArray pingRequest(qint64 clientTimeMs) {
    QByteArray output;
    appendInt64(output, 1, clientTimeMs);
    return output;
}

QByteArray listAuditLogsRequest(int page, int pageSize, const QString& actor) {
    QByteArray output;
    appendInt64(output, 1, page);
    appendInt64(output, 2, pageSize);
    if (!actor.isEmpty()) appendString(output, 3, actor);
    return output;
}

QByteArray statsRequest() {
    return {};
}

bool decodeLoginResponse(const QByteArray& bytes, LoginResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if ((field == 1 || field == 2 || field == 6) && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            if (field == 1) response.success = value != 0;
            else if (field == 2) response.userId = static_cast<qint64>(value);
            else response.remainingAttempts = static_cast<int>(value);
            return true;
        }
        if ((field == 3 || field == 4) && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; if (field == 3) response.role = QString::fromUtf8(value); else response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeRegisterResponse(const QByteArray& bytes, RegisterResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.success = value != 0; return true; }
        if (field == 2 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.userId = static_cast<qint64>(value); return true; }
        if (field == 3 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeQueryUserResponse(const QByteArray& bytes, QueryUserResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.found = value != 0; return true; }
        if (field == 2 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.userId = static_cast<qint64>(value); return true; }
        if (field >= 3 && field <= 6 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; if (field == 3) response.username = QString::fromUtf8(value); else if (field == 4) response.role = QString::fromUtf8(value); else if (field == 5) response.message = QString::fromUtf8(value); else response.status = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeCheckPermissionResponse(const QByteArray& bytes, CheckPermissionResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.allowed = value != 0; return true; }
        if (field == 2 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeOperationResponse(const QByteArray& bytes, OperationResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.success = value != 0; return true; }
        if (field == 2 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeResetPasswordResponse(const QByteArray& bytes, ResetPasswordResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.success = value != 0; return true; }
        if ((field == 2 || field == 3) && wire == 2) {
            QByteArray value;
            if (!readBytes(input, offset, value)) return false;
            if (field == 2) response.message = QString::fromUtf8(value);
            else response.generatedPassword = QString::fromUtf8(value);
            return true;
        }
        return skipField(input, offset, wire);
    });
}

bool decodePongResponse(const QByteArray& bytes, PongInfo& pong) {
    return parseMessage(bytes, [&pong](int field, int wire, const QByteArray& input, int& offset) {
        if ((field == 1 || field == 2) && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            if (field == 1) pong.clientTimeMs = static_cast<qint64>(value);
            else pong.serverTimeMs = static_cast<qint64>(value);
            return true;
        }
        return skipField(input, offset, wire);
    });
}

bool decodeListAuditLogsResponse(const QByteArray& bytes, ListAuditLogsResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if ((field == 1 || (field >= 4 && field <= 6)) && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            if (field == 1) response.success = value != 0;
            else if (field == 4) response.total = static_cast<int>(value);
            else if (field == 5) response.page = static_cast<int>(value);
            else response.pageSize = static_cast<int>(value);
            return true;
        }
        if (field == 2 && wire == 2) {
            QByteArray nested;
            if (!readBytes(input, offset, nested)) return false;
            AuditLogInfo item;
            if (!parseMessage(nested, [&item](int nestedField, int nestedWire, const QByteArray& nestedInput, int& nestedOffset) {
                if ((nestedField == 1 || nestedField == 8) && nestedWire == 0) {
                    std::uint64_t value = 0;
                    if (!readVarint(nestedInput, nestedOffset, value)) return false;
                    if (nestedField == 1) item.id = static_cast<qint64>(value);
                    else item.createdAt = static_cast<qint64>(value);
                    return true;
                }
                if (nestedField >= 2 && nestedField <= 7 && nestedWire == 2) {
                    QByteArray value;
                    if (!readBytes(nestedInput, nestedOffset, value)) return false;
                    const auto text = QString::fromUtf8(value);
                    if (nestedField == 2) item.actor = text;
                    else if (nestedField == 3) item.action = text;
                    else if (nestedField == 4) item.target = text;
                    else if (nestedField == 5) item.result = text;
                    else if (nestedField == 6) item.detail = text;
                    else item.actorIp = text;
                    return true;
                }
                return skipField(nestedInput, nestedOffset, nestedWire);
            })) return false;
            response.entries.push_back(item);
            return true;
        }
        if (field == 3 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeListUsersResponse(const QByteArray& bytes, ListUsersResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if ((field == 1 || (field >= 4 && field <= 6)) && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            if (field == 1) response.success = value != 0;
            else if (field == 4) response.total = static_cast<int>(value);
            else if (field == 5) response.page = static_cast<int>(value);
            else response.pageSize = static_cast<int>(value);
            return true;
        }
        if (field == 2 && wire == 2) {
            QByteArray nested;
            if (!readBytes(input, offset, nested)) return false;
            UserInfo item;
            if (!parseMessage(nested, [&item](int nestedField, int nestedWire, const QByteArray& nestedInput, int& nestedOffset) {
                if (nestedField == 1 && nestedWire == 0) { std::uint64_t value = 0; if (!readVarint(nestedInput, nestedOffset, value)) return false; item.userId = static_cast<qint64>(value); return true; }
                if ((nestedField >= 2 && nestedField <= 4) && nestedWire == 2) { QByteArray value; if (!readBytes(nestedInput, nestedOffset, value)) return false; if (nestedField == 2) item.username = QString::fromUtf8(value); else if (nestedField == 3) item.role = QString::fromUtf8(value); else item.status = QString::fromUtf8(value); return true; }
                return skipField(nestedInput, nestedOffset, nestedWire);
            })) return false;
            response.users.push_back(item);
            return true;
        }
        if (field == 3 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeListOnlineResponse(const QByteArray& bytes, ListOnlineResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) { std::uint64_t value = 0; if (!readVarint(input, offset, value)) return false; response.success = value != 0; return true; }
        if (field == 2 && wire == 2) {
            QByteArray nested;
            if (!readBytes(input, offset, nested)) return false;
            OnlineSessionInfo item;
            if (!parseMessage(nested, [&item](int nestedField, int nestedWire, const QByteArray& nestedInput, int& nestedOffset) {
                if ((nestedField >= 1 && nestedField <= 3) || nestedField == 5) {
                    if (nestedWire != 2) return skipField(nestedInput, nestedOffset, nestedWire);
                    QByteArray value;
                    if (!readBytes(nestedInput, nestedOffset, value)) return false;
                    if (nestedField == 1) item.sessionId = QString::fromUtf8(value);
                    else if (nestedField == 2) item.username = QString::fromUtf8(value);
                    else if (nestedField == 3) item.role = QString::fromUtf8(value);
                    else item.peerAddress = QString::fromUtf8(value);
                    return true;
                }
                if (nestedField == 4 && nestedWire == 0) {
                    std::uint64_t value = 0;
                    if (!readVarint(nestedInput, nestedOffset, value)) return false;
                    item.authenticated = value != 0;
                    return true;
                }
                return skipField(nestedInput, nestedOffset, nestedWire);
            })) return false;
            response.sessions.push_back(item);
            return true;
        }
        if (field == 3 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; response.message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

bool decodeStatsResponse(const QByteArray& bytes, StatsInfo& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if ((field == 1 || (field >= 4 && field <= 8)) && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            if (field == 1) response.success = value != 0;
            else if (field == 4) response.uptimeSeconds = static_cast<qint64>(value);
            else if (field == 5) response.currentConnections = static_cast<int>(value);
            else if (field == 6) response.totalConnections = static_cast<qint64>(value);
            else if (field == 7) response.ioThreads = static_cast<int>(value);
            else if (field == 8) response.workerThreads = static_cast<int>(value);
            return true;
        }
        if (field == 10 && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            response.activeSessions = static_cast<int>(value);
            return true;
        }
        if ((field == 2 || field == 9) && wire == 2) {
            QByteArray value;
            if (!readBytes(input, offset, value)) return false;
            if (field == 2) response.message = QString::fromUtf8(value);
            else response.storage = QString::fromUtf8(value);
            return true;
        }
        return skipField(input, offset, wire);
    });
}

QByteArray whoamiRequest() {
    return {};
}

bool decodeWhoamiResponse(const QByteArray& bytes, AccountInfo& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if ((field == 1 || (field >= 4 && field <= 11)) && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            if (field == 1) response.success = value != 0;
            else if (field == 4) response.userId = static_cast<qint64>(value);
            else if (field == 9) response.authenticatedAt = static_cast<qint64>(value);
            else if (field == 10) response.lastActiveAt = static_cast<qint64>(value);
            else if (field == 11) response.serverTimeSeconds = static_cast<qint64>(value);
            return true;
        }
        if ((field == 2 || (field >= 5 && field <= 8)) && wire == 2) {
            QByteArray value;
            if (!readBytes(input, offset, value)) return false;
            const auto text = QString::fromUtf8(value);
            if (field == 2) response.message = text;
            else if (field == 5) response.username = text;
            else if (field == 6) response.role = text;
            else if (field == 7) response.status = text;
            else response.peerAddress = text;
            return true;
        }
        return skipField(input, offset, wire);
    });
}

QByteArray mySessionsRequest() {
    return {};
}

bool decodeMySessionsResponse(const QByteArray& bytes, MySessionsResponse& response) {
    return parseMessage(bytes, [&response](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            response.success = value != 0;
            return true;
        }
        if (field == 2 && wire == 2) {
            QByteArray value;
            if (!readBytes(input, offset, value)) return false;
            response.message = QString::fromUtf8(value);
            return true;
        }
        if (field == 5 && wire == 0) {
            std::uint64_t value = 0;
            if (!readVarint(input, offset, value)) return false;
            response.total = static_cast<int>(value);
            return true;
        }
        if (field == 4 && wire == 2) {
            QByteArray nested;
            if (!readBytes(input, offset, nested)) return false;
            MySessionInfo item;
            if (!parseMessage(nested, [&item](int nestedField, int nestedWire, const QByteArray& nestedInput, int& nestedOffset) {
                if ((nestedField == 1 || nestedField == 2) && nestedWire == 2) {
                    QByteArray value;
                    if (!readBytes(nestedInput, nestedOffset, value)) return false;
                    if (nestedField == 1) item.sessionId = QString::fromUtf8(value);
                    else item.peerAddress = QString::fromUtf8(value);
                    return true;
                }
                if ((nestedField >= 3 && nestedField <= 5) && nestedWire == 0) {
                    std::uint64_t value = 0;
                    if (!readVarint(nestedInput, nestedOffset, value)) return false;
                    if (nestedField == 3) item.authenticatedAt = static_cast<qint64>(value);
                    else if (nestedField == 4) item.lastActiveAt = static_cast<qint64>(value);
                    else item.current = value != 0;
                    return true;
                }
                return skipField(nestedInput, nestedOffset, nestedWire);
            })) return false;
            response.sessions.push_back(item);
            return true;
        }
        return skipField(input, offset, wire);
    });
}

bool decodeErrorResponse(const QByteArray& bytes, QString& message) {
    return parseMessage(bytes, [&message](int field, int wire, const QByteArray& input, int& offset) {
        if (field == 1 && wire == 2) { QByteArray value; if (!readBytes(input, offset, value)) return false; message = QString::fromUtf8(value); return true; }
        return skipField(input, offset, wire);
    });
}

}  // namespace qtproto
