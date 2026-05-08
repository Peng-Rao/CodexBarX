#include "ProviderUIService.h"
#include "Localization.h"
#include "UsageBackend.h"
#include "UsageBackendTypes.h"
#include "../providers/ProviderStatusManager.h"
#include "../providers/ProviderCredentialManager.h"
#include "../providers/ProviderCatalogSnapshot.h"
#include "../account/TokenAccountStore.h"
#include "../app/SettingsStore.h"

#include <QMetaObject>

namespace {

struct ProviderListBuildItem {
    QString id;
    bool enabled = false;
    QString name;
    QString sessionLabel;
    QString weeklyLabel;
    bool supportsCredits = false;
    QString dashboardURL;
    QString statusPageURL;
    QString statusLinkURL;
    QString statusWorkspaceProductID;
    QString brandColor;
    QVector<QString> sourceModes;
    bool supportsMultipleAccounts = false;
    QVector<QString> requiredCredentialTypes;
    QString defaultTokenAccount;
    int tokenAccountCount = 0;
    bool hasUsage = false;
    double usagePercent = 0.0;
    QString status;
};

QVariantList buildProviderListFromItems(QVector<ProviderListBuildItem> items,
                                        const QStringList& order)
{
    if (!order.isEmpty()) {
        std::sort(items.begin(), items.end(), [&](const ProviderListBuildItem& a,
                                                  const ProviderListBuildItem& b) {
            int idxA = order.indexOf(a.id);
            int idxB = order.indexOf(b.id);
            if (idxA == -1 && idxB == -1) return a.id < b.id;
            if (idxA == -1) return false;
            if (idxB == -1) return true;
            return idxA < idxB;
        });
    } else {
        std::sort(items.begin(), items.end(), [](const ProviderListBuildItem& a,
                                                 const ProviderListBuildItem& b) {
            return a.id < b.id;
        });
    }

    QVariantList list;
    for (const auto& item : items) {
        QVariantMap entry;
        entry[QStringLiteral("id")] = item.id;
        entry[QStringLiteral("enabled")] = item.enabled;
        entry[QStringLiteral("name")] = item.name;
        entry[QStringLiteral("sessionLabel")] = Localization::providerLabel(item.sessionLabel);
        entry[QStringLiteral("weeklyLabel")] = Localization::providerLabel(item.weeklyLabel);
        entry[QStringLiteral("supportsCredits")] = item.supportsCredits;
        entry[QStringLiteral("dashboardURL")] = item.dashboardURL;
        entry[QStringLiteral("statusPageURL")] = item.statusPageURL;
        entry[QStringLiteral("statusLinkURL")] = item.statusLinkURL;
        entry[QStringLiteral("statusWorkspaceProductID")] = item.statusWorkspaceProductID;
        entry[QStringLiteral("brandColor")] = item.brandColor;

        QVariantList sourceModes;
        for (const auto& mode : item.sourceModes) sourceModes.append(mode);
        entry[QStringLiteral("sourceModes")] = sourceModes;

        QVariantMap tokenAccount;
        tokenAccount[QStringLiteral("supportsMultipleAccounts")] = item.supportsMultipleAccounts;
        QVariantList requiredCredentials;
        for (const auto& credential : item.requiredCredentialTypes) {
            requiredCredentials.append(credential);
        }
        tokenAccount[QStringLiteral("requiredCredentialTypes")] = requiredCredentials;
        entry[QStringLiteral("tokenAccount")] = tokenAccount;
        entry[QStringLiteral("defaultTokenAccount")] = item.defaultTokenAccount;
        entry[QStringLiteral("tokenAccountCount")] = item.tokenAccountCount;

        if (item.hasUsage) {
            QVariantMap usage;
            usage[QStringLiteral("percent")] = item.usagePercent;
            usage[QStringLiteral("remaining")] = 100.0 - item.usagePercent;
            entry[QStringLiteral("usage")] = usage;
        }

        if (!item.status.isEmpty()) {
            entry[QStringLiteral("status")] = item.status;
        }

        list.append(entry);
    }
    return list;
}

struct ProviderSettingFieldBuildInput {
    ProviderSettingsDescriptor descriptor;
    QVariant value;
    QVariantMap secretStatus;
};

QVariantList buildProviderSettingsFieldsFromInputs(const QVector<ProviderSettingFieldBuildInput>& inputs)
{
    QVariantList list;
    for (const auto& input : inputs) {
        const auto& d = input.descriptor;
        QVariantMap field;
        field[QStringLiteral("key")] = d.key;
        field[QStringLiteral("label")] = Localization::providerSettingLabel(d.label);
        field[QStringLiteral("type")] = d.type;
        field[QStringLiteral("defaultValue")] = d.defaultValue;
        field[QStringLiteral("value")] = d.sensitive ? QVariant() : input.value;
        field[QStringLiteral("credentialTarget")] = d.credentialTarget;
        field[QStringLiteral("envVar")] = d.envVar;
        field[QStringLiteral("placeholder")] = d.placeholder;
        field[QStringLiteral("helpText")] = d.helpText;
        field[QStringLiteral("multiline")] = d.multiline;
        field[QStringLiteral("sensitive")] = d.sensitive;
        if (d.sensitive) {
            field[QStringLiteral("secretStatus")] = input.secretStatus;
        }

        QVariantList options;
        for (const auto& option : d.options) {
            QVariantMap opt;
            opt[QStringLiteral("value")] = option.value;
            opt[QStringLiteral("label")] = Localization::providerSettingLabel(option.label);
            options.append(opt);
        }
        field[QStringLiteral("options")] = options;
        list.append(field);
    }
    return list;
}

struct ProviderDescriptorBuildInput {
    QString providerId;
    bool hasDescriptor = false;
    ProviderDescriptor descriptor;
    bool enabled = false;
    QString brandColor;
    QString statusURL;
    QString defaultTokenAccount;
    int tokenAccountCount = 0;
    QVector<ProviderSettingFieldBuildInput> settingsFields;
};

QVariantMap buildProviderDescriptorFromInput(const ProviderDescriptorBuildInput& input)
{
    QVariantMap data;
    if (!input.hasDescriptor) return data;

    const auto& desc = input.descriptor;
    data[QStringLiteral("id")] = desc.id;
    data[QStringLiteral("displayName")] = desc.metadata.displayName;
    data[QStringLiteral("sessionLabel")] = Localization::providerLabel(desc.metadata.sessionLabel);
    data[QStringLiteral("weeklyLabel")] = Localization::providerLabel(desc.metadata.weeklyLabel);
    data[QStringLiteral("dashboardURL")] = desc.metadata.dashboardURL;
    data[QStringLiteral("subscriptionDashboardURL")] = desc.metadata.subscriptionDashboardURL;
    data[QStringLiteral("statusPageURL")] = desc.metadata.statusPageURL;
    data[QStringLiteral("statusLinkURL")] = desc.metadata.statusLinkURL;
    data[QStringLiteral("statusWorkspaceProductID")] = desc.metadata.statusWorkspaceProductID;
    data[QStringLiteral("statusURL")] = input.statusURL;
    data[QStringLiteral("supportsCredits")] = desc.metadata.supportsCredits;
    data[QStringLiteral("cliName")] = desc.metadata.cliName;
    data[QStringLiteral("enabled")] = input.enabled;
    data[QStringLiteral("settingsFields")] = buildProviderSettingsFieldsFromInputs(input.settingsFields);
    data[QStringLiteral("brandColor")] = input.brandColor;

    QVariantList modes;
    for (const auto& mode : desc.fetchPlan.allowedSourceModes) modes.append(mode);
    data[QStringLiteral("sourceModes")] = modes;
    data[QStringLiteral("defaultSourceMode")] = desc.fetchPlan.defaultSourceMode;

    QVariantMap tokenAccount;
    tokenAccount[QStringLiteral("supportsMultipleAccounts")] = desc.tokenAccounts.supportsMultipleAccounts;
    QVariantList requiredCredentials;
    for (const auto& credential : desc.tokenAccounts.requiredCredentialTypes) {
        requiredCredentials.append(credential);
    }
    tokenAccount[QStringLiteral("requiredCredentialTypes")] = requiredCredentials;
    data[QStringLiteral("tokenAccount")] = tokenAccount;
    data[QStringLiteral("supportsMultipleAccounts")] = desc.tokenAccounts.supportsMultipleAccounts;
    data[QStringLiteral("defaultTokenAccount")] = input.defaultTokenAccount;
    data[QStringLiteral("tokenAccountCount")] = input.tokenAccountCount;
    return data;
}

} // namespace

ProviderUIService::ProviderUIService(QObject* parent)
    : QObject(parent)
{
}

QVariantList ProviderUIService::providerList() const
{
    if (!m_providerListCacheValid && !m_providerListRefreshQueued) {
        QMetaObject::invokeMethod(const_cast<ProviderUIService*>(this),
                                  &ProviderUIService::requestProviderList,
                                  Qt::QueuedConnection);
    }
    return m_providerListCache;
}

void ProviderUIService::requestProviderList()
{
    if (!m_catalog || !m_backend) {
        return;
    }
    if (m_providerListRefreshQueued) {
        return;
    }
    m_providerListRefreshQueued = true;
    const int generation = ++m_listGeneration;

    QVector<ProviderListBuildItem> items;
    for (const auto& provider : m_catalog->providers()) {
        const QString id = provider.id;
        ProviderListBuildItem item;
        item.id = id;
        item.enabled = provider.enabled;
        if (provider.hasDescriptor) {
            const auto& desc = provider.descriptor;
            item.name = desc.metadata.displayName;
            item.sessionLabel = desc.metadata.sessionLabel;
            item.weeklyLabel = desc.metadata.weeklyLabel;
            item.supportsCredits = desc.metadata.supportsCredits;
            item.dashboardURL = desc.metadata.dashboardURL;
            item.statusPageURL = desc.metadata.statusPageURL;
            item.statusLinkURL = desc.metadata.statusLinkURL;
            item.statusWorkspaceProductID = desc.metadata.statusWorkspaceProductID;
            item.brandColor = provider.brandColor;
            item.sourceModes = desc.fetchPlan.allowedSourceModes;
            item.supportsMultipleAccounts = desc.tokenAccounts.supportsMultipleAccounts;
            item.requiredCredentialTypes = desc.tokenAccounts.requiredCredentialTypes;
            item.defaultTokenAccount = TokenAccountStore::instance()->defaultAccountId(id);
            item.tokenAccountCount = TokenAccountStore::instance()->accountCountForProvider(id);
        } else {
            item.name = id;
            item.sessionLabel = QStringLiteral("Session");
            item.weeklyLabel = QStringLiteral("Weekly");
            item.supportsCredits = false;
        }

        // Use snapshot accessor if available
        if (m_snapshotAccessor) {
            auto usagePercent = m_snapshotAccessor(id);
            if (usagePercent.has_value()) {
                item.hasUsage = true;
                item.usagePercent = usagePercent.value();
            }
        }

        // Get status from status manager
        if (m_statusManager) {
            const QVariantMap statusMap = m_statusManager->status(id);
            const QString statusState = statusMap.value(QStringLiteral("state"), QStringLiteral("unknown")).toString();
            if (statusState != QStringLiteral("unknown")) {
                item.status = statusState;
            }
        }

        items.append(item);
    }

    const QStringList order = m_settingsStore ? m_settingsStore->providerOrder() : QStringList();
    m_backend->dispatchValueJob(QStringLiteral("providerListModel"), generation,
                                [items, order]() -> QVariant {
        ProviderListPayload payload;
        payload.providers = buildProviderListFromItems(items, order);
        return QVariant::fromValue(payload);
    });
}

QVariantMap ProviderUIService::providerDescriptorData(const QString& id) const
{
    if (m_descriptorCache.contains(id)) {
        return m_descriptorCache.value(id);
    }
    if (!m_descriptorRefreshQueued.contains(id)) {
        QMetaObject::invokeMethod(const_cast<ProviderUIService*>(this),
                                  "requestProviderDescriptor",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, id));
    }

    // Return placeholder
    QString displayName = id;
    if (m_displayNameAccessor) {
        displayName = m_displayNameAccessor(id);
    }
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("loading"), true},
        {QStringLiteral("displayName"), displayName},
        {QStringLiteral("settingsFields"), QVariantList{}}
    };
}

void ProviderUIService::requestProviderDescriptor(const QString& providerId)
{
    if (!m_catalog || !m_backend) {
        return;
    }
    if (providerId.isEmpty() || m_descriptorRefreshQueued.contains(providerId)) {
        return;
    }
    m_descriptorRefreshQueued.insert(providerId);
    const int generation = m_descriptorGenerations.value(providerId, 0) + 1;
    m_descriptorGenerations.insert(providerId, generation);

    ProviderDescriptorBuildInput input;
    input.providerId = providerId;
    const auto catalogEntry = m_catalog->provider(providerId);
    if (catalogEntry.has_value()) {
        input.hasDescriptor = catalogEntry->hasDescriptor;
        input.descriptor = catalogEntry->descriptor;
        input.enabled = catalogEntry->enabled;
        input.brandColor = catalogEntry->brandColor;
    }

    // Use status URL accessor
    if (m_statusURLAccessor) {
        input.statusURL = m_statusURLAccessor(providerId);
    }

    input.defaultTokenAccount = TokenAccountStore::instance()->defaultAccountId(providerId);
    input.tokenAccountCount = TokenAccountStore::instance()->accountCountForProvider(providerId);

    if (catalogEntry.has_value()) {
        for (const auto& d : catalogEntry->settingsDescriptors) {
            ProviderSettingFieldBuildInput field;
            field.descriptor = d;
            field.value = d.sensitive
                ? QVariant()
                : (m_settingsStore ? m_settingsStore->providerSetting(providerId, d.key, d.defaultValue)
                                   : d.defaultValue);
            if (d.sensitive && m_secretStatusAccessor) {
                field.secretStatus = m_secretStatusAccessor(providerId, d.key);
            }
            input.settingsFields.append(field);
        }
    }

    m_backend->dispatchValueJob(QStringLiteral("providerDescriptorData"), generation,
                                [input]() -> QVariant {
        ProviderDescriptorDataPayload payload;
        payload.providerId = input.providerId;
        payload.descriptor = buildProviderDescriptorFromInput(input);
        return QVariant::fromValue(payload);
    });
}

void ProviderUIService::invalidateProviderListCache()
{
    m_providerListCacheValid = false;
}

void ProviderUIService::invalidateDescriptorCache(const QString& providerId)
{
    m_descriptorCache.remove(providerId);
}

void ProviderUIService::invalidateAllCaches()
{
    m_providerListCacheValid = false;
    m_providerListCache.clear();
    m_descriptorCache.clear();
}

void ProviderUIService::setCatalog(const ProviderCatalogSnapshot* catalog)
{
    m_catalog = catalog;
}

void ProviderUIService::setStatusManager(ProviderStatusManager* manager)
{
    m_statusManager = manager;
}

void ProviderUIService::setCredentialManager(ProviderCredentialManager* manager)
{
    m_credentialManager = manager;
}

void ProviderUIService::setSettingsStore(SettingsStore* store)
{
    m_settingsStore = store;
}

void ProviderUIService::setBackend(UsageBackend* backend)
{
    m_backend = backend;
}

void ProviderUIService::setSnapshotAccessor(SnapshotAccessor accessor)
{
    m_snapshotAccessor = std::move(accessor);
}

void ProviderUIService::setErrorAccessor(ErrorAccessor accessor)
{
    m_errorAccessor = std::move(accessor);
}

void ProviderUIService::setSecretStatusAccessor(SecretStatusAccessor accessor)
{
    m_secretStatusAccessor = std::move(accessor);
}

void ProviderUIService::setDisplayNameAccessor(DisplayNameAccessor accessor)
{
    m_displayNameAccessor = std::move(accessor);
}

void ProviderUIService::setStatusURLAccessor(StatusURLAccessor accessor)
{
    m_statusURLAccessor = std::move(accessor);
}

bool ProviderUIService::handleProviderListResult(int generation, const QVariantList& providers)
{
    if (generation != m_listGeneration) {
        return false;  // Stale result
    }
    m_providerListRefreshQueued = false;
    m_providerListCache = providers;
    m_providerListCacheValid = true;
    emit providerListModelChanged();
    return true;
}

bool ProviderUIService::handleDescriptorResult(const QString& providerId, int generation, const QVariantMap& descriptor)
{
    if (generation != m_descriptorGenerations.value(providerId)) {
        return false;  // Stale result
    }
    m_descriptorRefreshQueued.remove(providerId);
    if (!descriptor.isEmpty()) {
        m_descriptorCache.insert(providerId, descriptor);
    }
    emit providerDescriptorChanged(providerId);
    return true;
}

QVariantList ProviderUIService::buildProviderListNow() const
{
    if (!m_catalog) {
        return {};
    }

    QVector<ProviderListBuildItem> items;
    for (const auto& provider : m_catalog->providers()) {
        const QString id = provider.id;
        ProviderListBuildItem item;
        item.id = id;
        item.enabled = provider.enabled;
        if (provider.hasDescriptor) {
            const auto& desc = provider.descriptor;
            item.name = desc.metadata.displayName;
            item.sessionLabel = desc.metadata.sessionLabel;
            item.weeklyLabel = desc.metadata.weeklyLabel;
            item.supportsCredits = desc.metadata.supportsCredits;
            item.dashboardURL = desc.metadata.dashboardURL;
            item.statusPageURL = desc.metadata.statusPageURL;
            item.statusLinkURL = desc.metadata.statusLinkURL;
            item.statusWorkspaceProductID = desc.metadata.statusWorkspaceProductID;
            item.brandColor = provider.brandColor;
            item.sourceModes = desc.fetchPlan.allowedSourceModes;
            item.supportsMultipleAccounts = desc.tokenAccounts.supportsMultipleAccounts;
            item.requiredCredentialTypes = desc.tokenAccounts.requiredCredentialTypes;
            item.defaultTokenAccount = TokenAccountStore::instance()->defaultAccountId(id);
            item.tokenAccountCount = TokenAccountStore::instance()->accountCountForProvider(id);
        } else {
            item.name = id;
            item.sessionLabel = QStringLiteral("Session");
            item.weeklyLabel = QStringLiteral("Weekly");
            item.supportsCredits = false;
        }

        if (m_snapshotAccessor) {
            auto usagePercent = m_snapshotAccessor(id);
            if (usagePercent.has_value()) {
                item.hasUsage = true;
                item.usagePercent = usagePercent.value();
            }
        }

        if (m_statusManager) {
            const QVariantMap statusMap = m_statusManager->status(id);
            const QString statusState = statusMap.value(QStringLiteral("state"), QStringLiteral("unknown")).toString();
            if (statusState != QStringLiteral("unknown")) {
                item.status = statusState;
            }
        }

        items.append(item);
    }

    const QStringList order = m_settingsStore ? m_settingsStore->providerOrder() : QStringList();
    const QVariantList list = buildProviderListFromItems(items, order);
    m_providerListCacheValid = true;
    m_providerListCache = list;
    return list;
}

QVariantMap ProviderUIService::providerUsageSnapshot(const QString& providerId, const UsageSnapshot& snap) const
{
    QVariantMap result;
    const bool showUsedPercent = m_settingsStore ? m_settingsStore->usageBarsShowUsed() : false;
    const bool isDetailProvider = (providerId == "deepseek" || providerId == "warp" || providerId == "kilo" || providerId == "abacus");

    auto metricMap = [&](const RateWindow& rw) {
        QVariantMap metric;
        const double remaining = rw.remainingPercent();
        metric["percent"] = showUsedPercent ? rw.usedPercent : remaining;
        metric["usedPercent"] = rw.usedPercent;
        metric["remaining"] = remaining;
        metric["displayIsUsed"] = showUsedPercent;
        if (rw.resetsAt.has_value() && rw.resetsAt.value().isValid()) {
            metric["resetsAt"] = rw.resetsAt.value().toString(Qt::ISODate);
        }
        return metric;
    };

    if (snap.primary.has_value()) {
        result["primary"] = metricMap(*snap.primary);
        if (isDetailProvider && snap.primary->resetDescription.has_value()) {
            QString detail = snap.primary->resetDescription.value().trimmed();
            if (!detail.isEmpty()) result["detail"] = detail;
        }
    }
    if (snap.secondary.has_value()) {
        result["secondary"] = metricMap(*snap.secondary);
    }
    if (snap.tertiary.has_value()) {
        result["tertiary"] = metricMap(*snap.tertiary);
    }
    return result;
}

QVariantMap ProviderUIService::buildDescriptorDataNow(const QString& id) const
{
    if (!m_catalog) {
        return {};
    }

    const auto catalogEntry = m_catalog->provider(id);
    if (!catalogEntry.has_value()) {
        return {};
    }

    ProviderDescriptorBuildInput input;
    input.providerId = id;
    input.hasDescriptor = catalogEntry->hasDescriptor;
    input.descriptor = catalogEntry->descriptor;
    input.enabled = catalogEntry->enabled;
    input.brandColor = catalogEntry->brandColor;

    if (m_statusURLAccessor) {
        input.statusURL = m_statusURLAccessor(id);
    }

    input.defaultTokenAccount = TokenAccountStore::instance()->defaultAccountId(id);
    input.tokenAccountCount = TokenAccountStore::instance()->accountCountForProvider(id);

    for (const auto& d : catalogEntry->settingsDescriptors) {
        ProviderSettingFieldBuildInput field;
        field.descriptor = d;
        field.value = d.sensitive
            ? QVariant()
            : (m_settingsStore ? m_settingsStore->providerSetting(id, d.key, d.defaultValue)
                               : d.defaultValue);
        if (d.sensitive && m_secretStatusAccessor) {
            field.secretStatus = m_secretStatusAccessor(id, d.key);
        }
        input.settingsFields.append(field);
    }

    const QVariantMap data = buildProviderDescriptorFromInput(input);
    m_descriptorCache.insert(id, data);
    return data;
}
