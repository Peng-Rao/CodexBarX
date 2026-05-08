#include "UsageDetailsViewModel.h"

#include "UsageStore.h"

UsageDetailsViewModel::UsageDetailsViewModel(UsageStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(16);
    connect(&m_syncTimer, &QTimer::timeout, this, &UsageDetailsViewModel::syncNow);

    if (!m_store) {
        return;
    }

    m_costUsageEnabled = m_store->costUsageEnabled();
    m_costUsageRefreshing = m_store->costUsageRefreshing();

    connect(m_store, &UsageStore::costUsageEnabledChanged,
            this, [this]() {
                syncCostFlags();
                scheduleSync();
            });
    connect(m_store, &UsageStore::costUsageRefreshingChanged,
            this, [this]() {
                syncCostFlags();
                scheduleSync();
            });
    connect(m_store, &UsageStore::costUsageChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::costUsageProviderDetailChanged,
            this, &UsageDetailsViewModel::syncProviderDetail);
    connect(m_store, &UsageStore::providerListModelChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::providerIDsChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::snapshotRevisionChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::statusRevisionChanged,
            this, &UsageDetailsViewModel::scheduleSync);
}

void UsageDetailsViewModel::activate()
{
    if (!m_store) {
        return;
    }

    if (!m_active) {
        m_active = true;
        emit activeChanged();
    }

    m_store->ensureCostUsageEnabled();
    m_store->requestCostUsageSummary();
    m_store->requestCostUsageDetailsRows();
    syncCostFlags();
    scheduleSync();
}

void UsageDetailsViewModel::deactivate()
{
    if (!m_active) {
        return;
    }

    m_active = false;
    emit activeChanged();
    if (m_store) {
        m_store->releaseCostUsageViewCaches();
    }
    if (!m_costData.isEmpty()) {
        m_costData.clear();
        emit costDataChanged();
    }
    if (!m_providerRows.isEmpty() || m_tokenProviderCount != 0) {
        m_providerRows.clear();
        m_tokenProviderCount = 0;
        emit providerRowsChanged();
    }
    if (!m_providerDetails.isEmpty()) {
        m_providerDetails.clear();
        emit providerDetailsChanged();
    }
}

void UsageDetailsViewModel::refreshCostUsage()
{
    if (m_store) {
        m_store->refreshCostUsage();
    }
}

void UsageDetailsViewModel::requestProviderDetail(const QString& providerId)
{
    if (!m_active || !m_store || providerId.isEmpty()) {
        return;
    }

    QVariantMap current = m_providerDetails.value(providerId).toMap();
    const QString state = current.value(QStringLiteral("state")).toString();
    if (state == QLatin1String("ready") || state == QLatin1String("loading")) {
        return;
    }

    current.insert(QStringLiteral("providerId"), providerId);
    current.insert(QStringLiteral("state"), QStringLiteral("loading"));
    current.insert(QStringLiteral("models"), QVariantList());
    m_providerDetails.insert(providerId, current);
    emit providerDetailsChanged();

    m_store->requestCostUsageProviderDetail(providerId);
}

void UsageDetailsViewModel::scheduleSync()
{
    if (!m_active || !m_store) {
        return;
    }
    if (!m_syncTimer.isActive()) {
        m_syncTimer.start();
    }
}

void UsageDetailsViewModel::syncNow()
{
    if (!m_active || !m_store) {
        return;
    }

    syncCostFlags();

    m_store->requestCostUsageSummary();
    m_store->requestCostUsageDetailsRows();
    const QVariantMap nextCostData = m_store->costUsageData();
    const QVariantList nextRows = m_store->costUsageDetailsRows();
    const int nextTokenProviderCount = m_store->costUsageTokenProviderCount();

    if (m_costData != nextCostData) {
        m_costData = nextCostData;
        emit costDataChanged();
    }

    if (m_providerRows != nextRows) {
        m_providerRows = nextRows;
        m_tokenProviderCount = nextTokenProviderCount;
        if (!m_providerDetails.isEmpty()) {
            m_providerDetails.clear();
            emit providerDetailsChanged();
        }
        emit providerRowsChanged();
    } else if (m_tokenProviderCount != nextTokenProviderCount) {
        m_tokenProviderCount = nextTokenProviderCount;
        emit providerRowsChanged();
    }
}

void UsageDetailsViewModel::syncCostFlags()
{
    if (!m_store) {
        return;
    }

    const bool nextEnabled = m_store->costUsageEnabled();
    if (m_costUsageEnabled != nextEnabled) {
        m_costUsageEnabled = nextEnabled;
        emit costUsageEnabledChanged();
    }

    const bool nextRefreshing = m_store->costUsageRefreshing();
    if (m_costUsageRefreshing != nextRefreshing) {
        m_costUsageRefreshing = nextRefreshing;
        emit costUsageRefreshingChanged();
    }
}

void UsageDetailsViewModel::syncProviderDetail(const QString& providerId)
{
    if (!m_active || !m_store || providerId.isEmpty()) {
        return;
    }

    QVariantMap next = m_store->costUsageProviderDetail(providerId);
    if (next.isEmpty()) {
        next.insert(QStringLiteral("providerId"), providerId);
        next.insert(QStringLiteral("state"), QStringLiteral("empty"));
        next.insert(QStringLiteral("models"), QVariantList());
    }

    if (m_providerDetails.value(providerId).toMap() == next) {
        return;
    }
    m_providerDetails.insert(providerId, next);
    emit providerDetailsChanged();
}
