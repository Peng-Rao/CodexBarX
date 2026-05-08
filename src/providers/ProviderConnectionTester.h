#pragma once

#include <QObject>
#include <QHash>
#include <QVariantMap>

class ProviderConnectionTester : public QObject {
    Q_OBJECT
public:
    explicit ProviderConnectionTester(QObject* parent = nullptr);

    // Connection test state access
    QVariantMap testState(const QString& providerId) const;
    void setTestState(const QString& providerId, const QVariantMap& state);
    void clearTestState(const QString& providerId);
    void clearAll();

signals:
    void testStateChanged(const QString& providerId);

private:
    QHash<QString, QVariantMap> m_connectionTests;
};
