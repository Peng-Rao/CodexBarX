#pragma once

#include <QObject>
#include <QHash>
#include <QVariantMap>
#include <QSharedPointer>
#include <QAtomicInt>

class ProviderLoginManager : public QObject {
    Q_OBJECT
public:
    explicit ProviderLoginManager(QObject* parent = nullptr);

    // Login state access
    QVariantMap loginState(const QString& providerId) const;
    void setLoginState(const QString& providerId, const QVariantMap& state);
    void clearLoginState(const QString& providerId);

    // Cancel flag management
    QSharedPointer<QAtomicInt> cancelFlag(const QString& providerId) const;
    void setCancelFlag(const QString& providerId, const QSharedPointer<QAtomicInt>& flag);
    void removeCancelFlag(const QString& providerId);

signals:
    void loginStateChanged(const QString& providerId);

private:
    QHash<QString, QVariantMap> m_loginStates;
    QHash<QString, QSharedPointer<QAtomicInt>> m_loginCancelFlags;
};
