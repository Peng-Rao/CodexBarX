#pragma once

#include <QByteArray>
#include <QString>
#include <optional>

class MacOSCredentialStore {
public:
    static bool write(const QString& target, const QString& username, const QByteArray& secret);
    static std::optional<QByteArray> read(const QString& target);
    static bool remove(const QString& target);
    static bool exists(const QString& target);
};
