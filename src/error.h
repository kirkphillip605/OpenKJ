#pragma once

#include <QString>
#include <QSqlError>
#include <QNetworkReply>

namespace okj {

enum class ErrorCode {
    None,
    Database,
    Network,
    Unknown
};

struct Error {
    ErrorCode code{ErrorCode::None};
    QString message;

    static Error fromSqlError(const QSqlError &err) {
        return {ErrorCode::Database, err.text()};
    }

    static Error fromNetworkError(QNetworkReply::NetworkError err, const QString &msg) {
        return {ErrorCode::Network, msg.isEmpty() ? QString::number(static_cast<int>(err)) : msg};
    }
};

} // namespace okj

