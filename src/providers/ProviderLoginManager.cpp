#include "ProviderLoginManager.h"

ProviderLoginManager::ProviderLoginManager(QObject* parent)
    : QObject(parent)
{
}

QVariantMap ProviderLoginManager::loginState(const QString& providerId) const
{
    return m_loginStates.value(providerId, QVariantMap{
        {QStringLiteral("state"), QStringLiteral("idle")}
    });
}

void ProviderLoginManager::setLoginState(const QString& providerId, const QVariantMap& state)
{
    m_loginStates[providerId] = state;
    emit loginStateChanged(providerId);
}

void ProviderLoginManager::clearLoginState(const QString& providerId)
{
    m_loginStates.remove(providerId);
}

QSharedPointer<QAtomicInt> ProviderLoginManager::cancelFlag(const QString& providerId) const
{
    return m_loginCancelFlags.value(providerId);
}

void ProviderLoginManager::setCancelFlag(const QString& providerId, const QSharedPointer<QAtomicInt>& flag)
{
    m_loginCancelFlags[providerId] = flag;
}

void ProviderLoginManager::removeCancelFlag(const QString& providerId)
{
    m_loginCancelFlags.remove(providerId);
}
