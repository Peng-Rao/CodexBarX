#include "ProviderConnectionTester.h"

ProviderConnectionTester::ProviderConnectionTester(QObject* parent)
    : QObject(parent)
{
}

QVariantMap ProviderConnectionTester::testState(const QString& providerId) const
{
    return m_connectionTests.value(providerId, QVariantMap{
        {QStringLiteral("state"), QStringLiteral("idle")},
        {QStringLiteral("message"), QString()},
        {QStringLiteral("details"), QString()},
        {QStringLiteral("startedAt"), 0},
        {QStringLiteral("finishedAt"), 0},
        {QStringLiteral("durationMs"), 0}
    });
}

void ProviderConnectionTester::setTestState(const QString& providerId, const QVariantMap& state)
{
    m_connectionTests[providerId] = state;
    emit testStateChanged(providerId);
}

void ProviderConnectionTester::clearTestState(const QString& providerId)
{
    m_connectionTests.remove(providerId);
}

void ProviderConnectionTester::clearAll()
{
    m_connectionTests.clear();
}
