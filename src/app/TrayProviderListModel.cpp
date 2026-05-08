#include "TrayProviderListModel.h"

#include "UsageStore.h"

TrayProviderListModel::TrayProviderListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(16);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &TrayProviderListModel::rebuildNow);
}

void TrayProviderListModel::setUsageStore(UsageStore* store)
{
    if (m_store == store) {
        return;
    }

    if (m_store) {
        disconnect(m_store, nullptr, this, nullptr);
    }

    m_store = store;

    if (m_store) {
        connect(m_store, &UsageStore::providerIDsChanged,
                this, &TrayProviderListModel::scheduleFullRebuild);
        connect(m_store, &UsageStore::snapshotRevisionChanged,
                this, &TrayProviderListModel::scheduleFullRebuild);
        connect(m_store, &UsageStore::snapshotChanged,
                this, &TrayProviderListModel::refreshProvider);
        connect(m_store, &UsageStore::tokenAccountsChanged,
                this, &TrayProviderListModel::refreshProvider);
        connect(m_store, &UsageStore::codexCreditsChanged,
                this, [this]() { refreshProvider(QStringLiteral("codex")); });
        connect(m_store, &UsageStore::codexFetchAttemptsChanged,
                this, [this]() { refreshProvider(QStringLiteral("codex")); });
        connect(m_store, &UsageStore::lastKnownSessionWindowSourceChanged,
                this, [this]() { refreshProvider(QStringLiteral("codex")); });
    }

    rebuildNow();
}

int TrayProviderListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_rows.size();
}

QVariant TrayProviderListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row& row = m_rows.at(index.row());
    switch (role) {
    case ProviderIdRole:
        return row.providerId;
    case SnapshotRole:
        return row.snap;
    case TokenAccountsRole:
        return row.tokenAccounts;
    case DefaultTokenAccountIdRole:
        return row.defaultTokenAccountId;
    case AccountOptionsRole:
        return row.accountOptions;
    case StatusUrlRole:
        return row.statusUrl;
    case DashboardRole:
        return row.dashboard;
    default:
        return {};
    }
}

QHash<int, QByteArray> TrayProviderListModel::roleNames() const
{
    return {
        {ProviderIdRole, "providerId"},
        {SnapshotRole, "snap"},
        {TokenAccountsRole, "tokenAccounts"},
        {DefaultTokenAccountIdRole, "defaultTokenAccountId"},
        {AccountOptionsRole, "accountOptions"},
        {StatusUrlRole, "statusUrl"},
        {DashboardRole, "dashboard"},
    };
}

void TrayProviderListModel::scheduleFullRebuild()
{
    if (!m_rebuildTimer.isActive()) {
        m_rebuildTimer.start();
    }
}

void TrayProviderListModel::refreshProvider(const QString& providerId)
{
    if (!m_store || providerId.isEmpty()) {
        return;
    }

    const auto it = m_indexByProvider.constFind(providerId);
    if (it == m_indexByProvider.constEnd()) {
        scheduleFullRebuild();
        return;
    }

    m_rows[it.value()] = buildRow(providerId);
    const QModelIndex changed = index(it.value(), 0);
    emit dataChanged(changed, changed, {
        SnapshotRole,
        TokenAccountsRole,
        DefaultTokenAccountIdRole,
        AccountOptionsRole,
        StatusUrlRole,
        DashboardRole,
    });
}

TrayProviderListModel::Row TrayProviderListModel::buildRow(const QString& providerId) const
{
    Row row;
    row.providerId = providerId;
    if (!m_store) {
        return row;
    }

    row.snap = m_store->snapshotData(providerId);
    row.tokenAccounts = m_store->tokenAccountsForProvider(providerId);
    row.defaultTokenAccountId = m_store->defaultTokenAccount(providerId);
    row.accountOptions = buildAccountOptions(row.tokenAccounts);
    row.statusUrl = m_store->providerStatusURL(providerId);
    row.dashboard = m_store->providerDashboardData(providerId);
    return row;
}

QVariantList TrayProviderListModel::buildAccountOptions(const QVariantList& tokenAccounts) const
{
    QVariantList result;
    QVariantMap providerDefault;
    providerDefault.insert(QStringLiteral("value"), QString());
    providerDefault.insert(QStringLiteral("label"), QStringLiteral("Provider default"));
    result.append(providerDefault);

    for (const QVariant& item : tokenAccounts) {
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
        result.append(option);
    }

    return result;
}

void TrayProviderListModel::rebuildNow()
{
    const int previousCount = m_rows.size();

    beginResetModel();
    m_rows.clear();
    m_indexByProvider.clear();

    if (m_store) {
        const QStringList ids = m_store->providerIDs();
        for (const QString& id : ids) {
            m_indexByProvider.insert(id, m_rows.size());
            m_rows.append(buildRow(id));
        }
    }

    endResetModel();
    emitCountIfChanged(previousCount);
}

void TrayProviderListModel::emitCountIfChanged(int previousCount)
{
    if (previousCount != m_rows.size()) {
        emit countChanged();
    }
}
