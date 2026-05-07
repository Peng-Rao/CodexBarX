#include "SettingsProvidersModel.h"

#include "UsageStore.h"

SettingsProviderListModel::SettingsProviderListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SettingsProviderListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_providers.size();
}

QVariant SettingsProviderListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_providers.size()) {
        return {};
    }

    const QVariantMap provider = m_providers.at(index.row()).toMap();
    switch (role) {
    case ProviderIdRole:
        return provider.value(QStringLiteral("id"));
    case NameRole:
        return provider.value(QStringLiteral("name"));
    case EnabledRole:
        return provider.value(QStringLiteral("enabled"));
    case BrandColorRole:
        return provider.value(QStringLiteral("brandColor"));
    case UsageRole:
        return provider.value(QStringLiteral("usage"));
    case StatusRole:
        return provider.value(QStringLiteral("status"), QStringLiteral("unknown"));
    case LastUpdatedRole:
        return provider.value(QStringLiteral("lastUpdated"));
    default:
        return {};
    }
}

QHash<int, QByteArray> SettingsProviderListModel::roleNames() const
{
    return {
        {ProviderIdRole, "providerId"},
        {NameRole, "name"},
        {EnabledRole, "enabled"},
        {BrandColorRole, "brandColor"},
        {UsageRole, "usage"},
        {StatusRole, "status"},
        {LastUpdatedRole, "lastUpdated"},
    };
}

void SettingsProviderListModel::setProviders(const QVariantList& providers)
{
    beginResetModel();
    m_providers = providers;
    endResetModel();
}

QString SettingsProviderListModel::providerIdAt(int row) const
{
    if (row < 0 || row >= m_providers.size()) {
        return {};
    }
    return m_providers.at(row).toMap().value(QStringLiteral("id")).toString();
}

SettingsProvidersModel::SettingsProvidersModel(UsageStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    m_selectedConnectionTest = {{"state", "idle"}, {"message", ""}, {"details", ""}, {"startedAt", 0}, {"finishedAt", 0}, {"durationMs", 0}};
    m_selectedProviderStatus = {{"state", "unknown"}};

    if (!m_store) {
        return;
    }

    connect(m_store, &UsageStore::providerIDsChanged,
            this, &SettingsProvidersModel::syncProviderList);
    connect(m_store, &UsageStore::providerListModelChanged,
            this, &SettingsProvidersModel::syncProviderList);
    connect(m_store, &UsageStore::providerDescriptorChanged,
            this, [this](const QString& providerId) {
        if (providerId == m_selectedProvider) {
            syncSelectedDescriptor();
        }
    });
    connect(m_store, &UsageStore::providerConnectionTestChanged,
            this, [this](const QString& providerId) {
        if (providerId == m_selectedProvider) {
            syncSelectedConnectionTest();
        }
        m_store->requestProviderList();
    });
    connect(m_store, &UsageStore::providerSecretChanged,
            this, [this](const QString& providerId, const QString&) {
        if (providerId == m_selectedProvider) {
            syncSelectedDescriptor();
        }
    });
    connect(m_store, &UsageStore::providerStatusChanged,
            this, [this](const QString& providerId) {
        if (providerId == m_selectedProvider) {
            syncSelectedStatus();
        }
        m_store->requestProviderList();
    });
    connect(m_store, &UsageStore::snapshotRevisionChanged,
            this, &SettingsProvidersModel::syncSelectedUsageSnapshot);
    connect(m_store, &UsageStore::tokenAccountsChanged,
            this, [this](const QString& providerId) {
        if (providerId == m_selectedProvider) {
            syncSelectedTokenAccounts();
            syncSelectedDescriptor();
        }
        m_store->requestProviderList();
    });
    connect(m_store, &UsageStore::tokenAccountOperationStateChanged,
            this, &SettingsProvidersModel::syncTokenOperationState);
    connect(m_store, &UsageStore::codexAccountStateChanged,
            this, &SettingsProvidersModel::syncCodexState);
    connect(m_store, &UsageStore::codexCreditsChanged,
            this, &SettingsProvidersModel::syncCodexProjection);
    connect(m_store, &UsageStore::snapshotChanged,
            this, [this](const QString& providerId) {
        if (providerId == QLatin1String("codex")) {
            syncCodexProjection();
        }
    });

    syncTokenOperationState();
    syncCodexState();
    syncCodexProjection();
}

void SettingsProvidersModel::requestOpenProvidersTab()
{
    if (!m_store) {
        return;
    }
    m_store->requestProviderList();
    syncProviderList();
    selectFirstProviderIfNeeded();
}

void SettingsProvidersModel::selectProvider(const QString& providerId)
{
    if (m_selectedProvider == providerId) {
        return;
    }

    m_selectedProvider = providerId;
    emit selectedProviderChanged();
    m_selectedDescriptor = {};
    emit selectedDescriptorChanged();
    setDetailState(providerId.isEmpty() ? QStringLiteral("idle") : QStringLiteral("loading"));

    requestSelectedDescriptor();
    syncSelectedConnectionTest();
    syncSelectedStatus();
    syncSelectedError();
    syncSelectedUsageSnapshot();
    syncSelectedTokenAccounts();
    syncCodexState();
    syncCodexProjection();
}

void SettingsProvidersModel::moveProvider(int fromIndex, int toIndex)
{
    if (m_store && fromIndex != toIndex && fromIndex >= 0 && toIndex >= 0) {
        m_store->moveProvider(fromIndex, toIndex);
        m_store->requestProviderList();
    }
}

void SettingsProvidersModel::setProviderEnabled(const QString& providerId, bool enabled)
{
    if (!m_store) {
        return;
    }
    m_store->setProviderEnabled(providerId, enabled);
    if (m_selectedProvider == providerId) {
        requestSelectedDescriptor();
    }
    m_store->requestProviderList();
}

void SettingsProvidersModel::testConnection(const QString& providerId)
{
    if (!m_store) {
        return;
    }
    m_store->testProviderConnection(providerId);
    if (providerId == m_selectedProvider) {
        syncSelectedConnectionTest();
    }
}

void SettingsProvidersModel::refreshProvider(const QString& providerId)
{
    if (m_store) {
        m_store->refreshProvider(providerId);
    }
}

void SettingsProvidersModel::setProviderSetting(const QString& providerId, const QString& key, const QVariant& value)
{
    if (m_store) {
        m_store->setProviderSetting(providerId, key, value);
        if (providerId == m_selectedProvider) {
            requestSelectedDescriptor();
        }
    }
}

void SettingsProvidersModel::setProviderSecret(const QString& providerId, const QString& key, const QString& value)
{
    if (m_store) {
        m_store->setProviderSecret(providerId, key, value);
    }
}

void SettingsProvidersModel::clearProviderSecret(const QString& providerId, const QString& key)
{
    if (m_store) {
        m_store->clearProviderSecret(providerId, key);
    }
}

void SettingsProvidersModel::requestAddTokenAccount(const QString& providerId, const QString& displayName, int sourceMode)
{
    if (m_store) {
        m_store->requestAddTokenAccount(providerId, displayName, sourceMode);
    }
}

void SettingsProvidersModel::requestAddTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey)
{
    if (m_store) {
        m_store->requestAddTokenAccountWithApiKey(providerId, displayName, sourceMode, apiKey);
    }
}

void SettingsProvidersModel::requestRemoveTokenAccount(const QString& accountId)
{
    if (m_store) {
        m_store->requestRemoveTokenAccount(accountId);
    }
}

void SettingsProvidersModel::requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId)
{
    if (m_store) {
        m_store->requestSetDefaultTokenAccount(providerId, accountId);
    }
}

void SettingsProvidersModel::requestSetTokenAccountSourceMode(const QString& accountId, int sourceMode)
{
    if (m_store) {
        m_store->requestSetTokenAccountSourceMode(accountId, sourceMode);
    }
}

void SettingsProvidersModel::requestSetTokenAccountVisibility(const QString& accountId, int visibility)
{
    if (m_store) {
        m_store->requestSetTokenAccountVisibility(accountId, visibility);
    }
}

void SettingsProvidersModel::setCodexActiveAccount(const QString& accountId)
{
    if (m_store) {
        m_store->setCodexActiveAccount(accountId);
    }
}

void SettingsProvidersModel::addCodexAccount()
{
    if (m_store) {
        m_store->addCodexAccount(QString(), QString());
    }
}

void SettingsProvidersModel::cancelCodexAuthentication()
{
    if (m_store) {
        m_store->cancelCodexAuthentication();
    }
}

void SettingsProvidersModel::removeCodexAccount(const QString& accountId)
{
    if (m_store) {
        m_store->removeCodexAccount(accountId);
    }
}

void SettingsProvidersModel::reauthenticateCodexAccount(const QString& accountId)
{
    if (m_store) {
        m_store->reauthenticateCodexAccount(accountId);
    }
}

void SettingsProvidersModel::promoteCodexAccount(const QString& accountId)
{
    if (m_store) {
        m_store->promoteCodexAccount(accountId);
    }
}

void SettingsProvidersModel::syncProviderList()
{
    if (!m_store) {
        return;
    }

    const QVariantList providers = m_store->providerList();
    m_providers.setProviders(providers);
    const int nextCount = providers.size();
    if (m_providerCount != nextCount) {
        m_providerCount = nextCount;
        emit providersChanged();
    } else {
        emit providersChanged();
    }
    selectFirstProviderIfNeeded();
}

void SettingsProvidersModel::requestSelectedDescriptor()
{
    if (!m_store || m_selectedProvider.isEmpty()) {
        return;
    }
    setDetailState(QStringLiteral("loading"));
    m_store->requestProviderDescriptor(m_selectedProvider);
    syncSelectedDescriptor();
}

void SettingsProvidersModel::syncSelectedDescriptor()
{
    if (!m_store || m_selectedProvider.isEmpty()) {
        m_selectedDescriptor = {};
        emit selectedDescriptorChanged();
        setDetailState(QStringLiteral("idle"));
        return;
    }

    const QVariantMap next = m_store->providerDescriptorData(m_selectedProvider);
    if (m_selectedDescriptor != next) {
        m_selectedDescriptor = next;
        emit selectedDescriptorChanged();
    }
    setDetailState(next.isEmpty() ? QStringLiteral("loading") : QStringLiteral("ready"));
    syncSelectedTokenAccounts();
}

void SettingsProvidersModel::syncSelectedConnectionTest()
{
    if (!m_store || m_selectedProvider.isEmpty()) {
        return;
    }
    const QVariantMap next = m_store->providerConnectionTest(m_selectedProvider);
    if (m_selectedConnectionTest != next) {
        m_selectedConnectionTest = next;
        emit selectedConnectionTestChanged();
    }
}

void SettingsProvidersModel::syncSelectedStatus()
{
    if (!m_store || m_selectedProvider.isEmpty()) {
        return;
    }
    const QVariantMap next = m_store->providerStatus(m_selectedProvider);
    if (m_selectedProviderStatus != next) {
        m_selectedProviderStatus = next;
        emit selectedProviderStatusChanged();
    }
}

void SettingsProvidersModel::syncSelectedError()
{
    if (!m_store || m_selectedProvider.isEmpty()) {
        return;
    }
    const QString next = m_store->providerError(m_selectedProvider);
    if (m_selectedProviderError != next) {
        m_selectedProviderError = next;
        emit selectedProviderErrorChanged();
    }
}

void SettingsProvidersModel::syncSelectedUsageSnapshot()
{
    if (!m_store || m_selectedProvider.isEmpty()) {
        return;
    }
    const QVariantMap next = m_store->providerUsageSnapshot(m_selectedProvider);
    if (m_selectedUsageSnapshot != next) {
        m_selectedUsageSnapshot = next;
        emit selectedUsageSnapshotChanged();
    }
}

void SettingsProvidersModel::syncSelectedTokenAccounts()
{
    QVariantList accounts;
    QString defaultAccount;
    if (m_store && !m_selectedProvider.isEmpty() && m_selectedProvider != QLatin1String("codex")) {
        accounts = m_store->tokenAccountsForProvider(m_selectedProvider);
        defaultAccount = m_store->defaultTokenAccount(m_selectedProvider);
    }

    if (m_selectedTokenAccounts != accounts || m_selectedDefaultTokenAccountId != defaultAccount) {
        m_selectedTokenAccounts = accounts;
        m_selectedDefaultTokenAccountId = defaultAccount;
        emit selectedTokenAccountsChanged();
    }
}

void SettingsProvidersModel::syncTokenOperationState()
{
    if (!m_store) {
        return;
    }
    const QVariantMap next = m_store->tokenAccountOperationState();
    if (m_tokenAccountOperationState != next) {
        m_tokenAccountOperationState = next;
        emit tokenAccountOperationStateChanged();
    }
}

void SettingsProvidersModel::syncCodexState()
{
    QVariantMap next;
    if (m_store && m_selectedProvider == QLatin1String("codex")) {
        next = m_store->codexAccountState();
    }
    if (m_codexAccountState != next) {
        m_codexAccountState = next;
        emit codexAccountStateChanged();
    }
}

void SettingsProvidersModel::syncCodexProjection()
{
    QVariantMap next;
    if (m_store && m_selectedProvider == QLatin1String("codex")) {
        next = m_store->codexConsumerProjectionData();
    }
    if (m_codexProjection != next) {
        m_codexProjection = next;
        emit codexProjectionChanged();
    }
}

void SettingsProvidersModel::setDetailState(const QString& state)
{
    if (m_detailState == state) {
        return;
    }
    m_detailState = state;
    emit detailStateChanged();
}

void SettingsProvidersModel::selectFirstProviderIfNeeded()
{
    if (!m_selectedProvider.isEmpty() || m_providerCount <= 0) {
        return;
    }
    selectProvider(m_providers.providerIdAt(0));
}
