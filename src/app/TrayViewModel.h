#pragma once

#include "TrayProviderListModel.h"

#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

class UsageStore;

class TrayViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(TrayProviderListModel* providers READ providers CONSTANT)
    Q_PROPERTY(int providerCount READ providerCount NOTIFY providerCountChanged)
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)
    Q_PROPERTY(bool costUsageEnabled READ costUsageEnabled NOTIFY costUsageEnabledChanged)
    Q_PROPERTY(bool costUsageRefreshing READ costUsageRefreshing NOTIFY costUsageRefreshingChanged)
    Q_PROPERTY(QVariantMap costData READ costData NOTIFY costDataChanged)
    Q_PROPERTY(QString selectedProviderID READ selectedProviderID WRITE setSelectedProviderID NOTIFY selectedProviderIDChanged)
    Q_PROPERTY(bool providerSwitching READ providerSwitching NOTIFY providerSwitchingChanged)
    Q_PROPERTY(QVariantList providerSwitcherList READ providerSwitcherList NOTIFY providerSwitcherListChanged)
    Q_PROPERTY(QVariantMap codexAccountState READ codexAccountState NOTIFY codexAccountStateChanged)
    Q_PROPERTY(int providerDataRevision READ providerDataRevision NOTIFY providerDataChanged)

public:
    explicit TrayViewModel(UsageStore* store, QObject* parent = nullptr);

    TrayProviderListModel* providers() { return &m_providers; }
    int providerCount() const { return m_providers.count(); }
    bool isRefreshing() const { return m_isRefreshing; }
    bool costUsageEnabled() const { return m_costUsageEnabled; }
    bool costUsageRefreshing() const { return m_costUsageRefreshing; }
    QVariantMap costData() const { return m_costData; }
    QString selectedProviderID() const { return m_selectedProviderID; }
    void setSelectedProviderID(const QString& id);
    bool providerSwitching() const { return m_providerSwitching; }
    QVariantList providerSwitcherList() const;
    QVariantMap codexAccountState() const;
    int providerDataRevision() const { return m_providerDataRevision; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshProvider(const QString& providerId);
    Q_INVOKABLE void ensureCostUsageEnabled();
    Q_INVOKABLE void requestCostUsageViewData();
    Q_INVOKABLE QVariantList providerCostUsageList();
    Q_INVOKABLE QString requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId);
    Q_INVOKABLE void selectProvider(const QString& providerId);
    Q_INVOKABLE QVariantMap providerData(const QString& providerId) const;
    Q_INVOKABLE void setCodexActiveAccount(const QString& accountID);

signals:
    void providerCountChanged();
    void isRefreshingChanged();
    void costUsageEnabledChanged();
    void costUsageRefreshingChanged();
    void costDataChanged();
    void providerCostRowsChanged();
    void selectedProviderIDChanged();
    void providerSwitchingChanged();
    void providerSwitcherListChanged();
    void codexAccountStateChanged();
    void providerDataChanged();

private:
    void syncRefreshing();
    void syncCostUsageEnabled();
    void syncCostUsageRefreshing();
    void syncCostData();

    QPointer<UsageStore> m_store;
    TrayProviderListModel m_providers;
    bool m_isRefreshing = false;
    bool m_costUsageEnabled = false;
    bool m_costUsageRefreshing = false;
    QVariantMap m_costData;
    QVariantList m_providerCostRows;
    QString m_selectedProviderID;
    bool m_providerSwitching = false;
    int m_providerDataRevision = 0;
};
