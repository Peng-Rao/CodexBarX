#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class UsageStore;

class UsageDetailsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool costUsageEnabled READ costUsageEnabled NOTIFY costUsageEnabledChanged)
    Q_PROPERTY(bool costUsageRefreshing READ costUsageRefreshing NOTIFY costUsageRefreshingChanged)
    Q_PROPERTY(QVariantMap costData READ costData NOTIFY costDataChanged)
    Q_PROPERTY(QVariantList providerRows READ providerRows NOTIFY providerRowsChanged)
    Q_PROPERTY(int tokenProviderCount READ tokenProviderCount NOTIFY providerRowsChanged)
    Q_PROPERTY(QVariantMap providerDetails READ providerDetails NOTIFY providerDetailsChanged)

public:
    explicit UsageDetailsViewModel(UsageStore* store, QObject* parent = nullptr);

    bool active() const { return m_active; }
    bool costUsageEnabled() const { return m_costUsageEnabled; }
    bool costUsageRefreshing() const { return m_costUsageRefreshing; }
    QVariantMap costData() const { return m_costData; }
    QVariantList providerRows() const { return m_providerRows; }
    int tokenProviderCount() const { return m_tokenProviderCount; }
    QVariantMap providerDetails() const { return m_providerDetails; }

    Q_INVOKABLE void activate();
    Q_INVOKABLE void deactivate();
    Q_INVOKABLE void refreshCostUsage();
    Q_INVOKABLE void requestProviderDetail(const QString& providerId);

signals:
    void activeChanged();
    void costUsageEnabledChanged();
    void costUsageRefreshingChanged();
    void costDataChanged();
    void providerRowsChanged();
    void providerDetailsChanged();

private:
    void scheduleSync();
    void syncNow();
    void syncCostFlags();
    void syncProviderDetail(const QString& providerId);

    QPointer<UsageStore> m_store;
    QTimer m_syncTimer;
    bool m_active = false;
    bool m_costUsageEnabled = false;
    bool m_costUsageRefreshing = false;
    QVariantMap m_costData;
    QVariantList m_providerRows;
    QVariantMap m_providerDetails;
    int m_tokenProviderCount = 0;
};
