#include "TrayViewModel.h"

#include "UsageStore.h"

TrayViewModel::TrayViewModel(UsageStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    m_providers.setUsageStore(store);
    connect(&m_providers, &TrayProviderListModel::countChanged,
            this, &TrayViewModel::providerCountChanged);

    if (!m_store) {
        return;
    }

    m_isRefreshing = m_store->isRefreshing();
    m_costUsageEnabled = m_store->costUsageEnabled();
    m_costUsageRefreshing = m_store->costUsageRefreshing();

    connect(m_store, &UsageStore::refreshingChanged,
            this, &TrayViewModel::syncRefreshing);
    connect(m_store, &UsageStore::costUsageEnabledChanged,
            this, &TrayViewModel::syncCostUsageEnabled);
    connect(m_store, &UsageStore::costUsageRefreshingChanged,
            this, &TrayViewModel::syncCostUsageRefreshing);
    connect(m_store, &UsageStore::costUsageChanged,
            this, &TrayViewModel::syncCostData);

    syncCostData();
}

void TrayViewModel::refresh()
{
    if (m_store) {
        m_store->refresh();
    }
}

void TrayViewModel::refreshProvider(const QString& providerId)
{
    if (m_store) {
        m_store->refreshProvider(providerId);
    }
}

void TrayViewModel::ensureCostUsageEnabled()
{
    if (m_store) {
        m_store->ensureCostUsageEnabled();
        syncCostUsageEnabled();
    }
}

void TrayViewModel::requestCostUsageViewData()
{
    if (m_store) {
        m_store->requestCostUsageViewData();
    }
}

QVariantList TrayViewModel::providerCostUsageList() const
{
    return m_providerCostRows;
}

QString TrayViewModel::requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId)
{
    return m_store ? m_store->requestSetDefaultTokenAccount(providerId, accountId) : QString();
}

void TrayViewModel::syncRefreshing()
{
    if (!m_store) {
        return;
    }
    const bool next = m_store->isRefreshing();
    if (m_isRefreshing == next) {
        return;
    }
    m_isRefreshing = next;
    emit isRefreshingChanged();
}

void TrayViewModel::syncCostUsageEnabled()
{
    if (!m_store) {
        return;
    }
    const bool next = m_store->costUsageEnabled();
    if (m_costUsageEnabled == next) {
        return;
    }
    m_costUsageEnabled = next;
    emit costUsageEnabledChanged();
}

void TrayViewModel::syncCostUsageRefreshing()
{
    if (!m_store) {
        return;
    }
    const bool next = m_store->costUsageRefreshing();
    if (m_costUsageRefreshing == next) {
        return;
    }
    m_costUsageRefreshing = next;
    emit costUsageRefreshingChanged();
}

void TrayViewModel::syncCostData()
{
    if (!m_store) {
        return;
    }

    const QVariantMap nextCostData = m_store->costUsageData();
    if (m_costData != nextCostData) {
        m_costData = nextCostData;
        emit costDataChanged();
    }

    const QVariantList nextProviderRows = m_store->providerCostUsageList();
    if (m_providerCostRows != nextProviderRows) {
        m_providerCostRows = nextProviderRows;
        emit providerCostRowsChanged();
    }
}
