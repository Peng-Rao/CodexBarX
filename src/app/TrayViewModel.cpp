#include "TrayViewModel.h"

#include "UsageStore.h"
#include <QTimer>

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
    connect(m_store, &UsageStore::providerIDsChanged,
            this, &TrayViewModel::providerSwitcherListChanged);
    connect(m_store, &UsageStore::providerDescriptorChanged,
            this, &TrayViewModel::providerSwitcherListChanged);
    connect(m_store, &UsageStore::snapshotChanged,
            this, &TrayViewModel::providerSwitcherListChanged);
    connect(m_store, &UsageStore::snapshotRevisionChanged,
            this, &TrayViewModel::providerSwitcherListChanged);
    connect(m_store, &UsageStore::codexAccountStateChanged,
            this, &TrayViewModel::codexAccountStateChanged);

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
        m_store->requestCostUsageSummary();
    }
}

QVariantList TrayViewModel::providerCostUsageList()
{
    if (m_store) {
        m_providerCostRows = m_store->providerCostUsageList();
    }
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
    emit providerCostRowsChanged();
}

void TrayViewModel::setSelectedProviderID(const QString& id)
{
    if (m_selectedProviderID == id) {
        return;
    }
    m_selectedProviderID = id;
    emit selectedProviderIDChanged();
}

QVariantList TrayViewModel::providerSwitcherList() const
{
    QVariantList result;

    QVariantMap overview;
    overview.insert(QStringLiteral("providerId"), QString());
    overview.insert(QStringLiteral("displayName"), QStringLiteral("Overview"));
    overview.insert(QStringLiteral("iconSource"), QString());
    overview.insert(QStringLiteral("weeklyRemaining"), QVariant());
    result.append(overview);

    if (!m_store) {
        return result;
    }

    const QStringList ids = m_store->providerIDs();
    for (const QString& id : ids) {
        if (!m_store->isProviderEnabled(id)) {
            continue;
        }
        QVariantMap item;
        item.insert(QStringLiteral("providerId"), id);
        QString displayName = m_store->providerDisplayName(id);
        if (displayName.isEmpty()) {
            displayName = id;
        }
        item.insert(QStringLiteral("displayName"), displayName);
        item.insert(QStringLiteral("iconSource"), QStringLiteral("qrc:/icons/ProviderIcon-%1.svg").arg(id));

        const QVariantMap snap = m_store->snapshotData(id);
        if (snap.contains(QStringLiteral("secondaryRemaining"))) {
            item.insert(QStringLiteral("weeklyRemaining"), snap.value(QStringLiteral("secondaryRemaining")));
        } else {
            item.insert(QStringLiteral("weeklyRemaining"), QVariant());
        }
        item.insert(QStringLiteral("hasUsage"), snap.value(QStringLiteral("hasUsage")).toBool());
        result.append(item);
    }
    return result;
}

void TrayViewModel::selectProvider(const QString& providerId)
{
    if (m_selectedProviderID == providerId) {
        return;
    }

    m_providerSwitching = true;
    emit providerSwitchingChanged();

    m_selectedProviderID = providerId;
    emit selectedProviderIDChanged();

    QTimer::singleShot(50, this, [this]() {
        m_providerSwitching = false;
        emit providerSwitchingChanged();
    });
}

QVariantMap TrayViewModel::providerData(const QString& providerId) const
{
    QVariantMap result;
    if (!m_store || providerId.isEmpty()) {
        return result;
    }

    result.insert(QStringLiteral("providerId"), providerId);
    result.insert(QStringLiteral("snap"), m_store->snapshotData(providerId));
    result.insert(QStringLiteral("tokenAccounts"), m_store->tokenAccountsForProvider(providerId));
    result.insert(QStringLiteral("defaultTokenAccountId"), m_store->defaultTokenAccount(providerId));

    QVariantList options;
    QVariantMap providerDefault;
    providerDefault.insert(QStringLiteral("value"), QString());
    providerDefault.insert(QStringLiteral("label"), QStringLiteral("Provider default"));
    options.append(providerDefault);

    const QVariantList accounts = m_store->tokenAccountsForProvider(providerId);
    for (const QVariant& item : accounts) {
        const QVariantMap account = item.toMap();
        if (account.value(QStringLiteral("visibility")).toString() == QLatin1String("archived")) {
            continue;
        }
        const QString accountId = account.value(QStringLiteral("accountId")).toString();
        QVariantMap option;
        option.insert(QStringLiteral("value"), accountId);
        option.insert(QStringLiteral("label"),
                      account.value(QStringLiteral("displayName")).toString().isEmpty()
                          ? accountId
                          : account.value(QStringLiteral("displayName")).toString());
        options.append(option);
    }
    result.insert(QStringLiteral("accountOptions"), options);
    result.insert(QStringLiteral("statusUrl"), m_store->providerStatusURL(providerId));
    result.insert(QStringLiteral("dashboard"), m_store->providerDashboardData(providerId));
    return result;
}

QVariantMap TrayViewModel::codexAccountState() const
{
    return m_store ? m_store->codexAccountState() : QVariantMap();
}

void TrayViewModel::setCodexActiveAccount(const QString& accountID)
{
    if (m_store) {
        m_store->setCodexActiveAccount(accountID);
    }
}
