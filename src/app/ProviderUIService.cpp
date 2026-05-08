#include "ProviderUIService.h"
#include "Localization.h"
#include "UsageBackend.h"
#include "UsageBackendTypes.h"
#include "../models/UsagePace.h"
#include "../providers/ProviderStatusManager.h"
#include "../providers/ProviderCredentialManager.h"
#include "../providers/ProviderCatalogSnapshot.h"
#include "../providers/codex/CodexConsumerProjection.h"
#include "../account/TokenAccountStore.h"
#include "../app/SettingsStore.h"
#include "../util/UsagePaceText.h"

#include <QDateTime>
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

QVector<ProviderListBuildItem> collectProviderListBuildItems(
    const ProviderCatalogSnapshot* catalog,
    const ProviderUIService::SnapshotAccessor& snapshotAccessor,
    ProviderStatusManager* statusManager)
{
    QVector<ProviderListBuildItem> items;
    if (!catalog) {
        return items;
    }

    for (const auto& provider : catalog->providers()) {
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

        if (snapshotAccessor) {
            auto usagePercent = snapshotAccessor(id);
            if (usagePercent.has_value()) {
                item.hasUsage = true;
                item.usagePercent = usagePercent.value();
            }
        }

        if (statusManager) {
            const QVariantMap statusMap = statusManager->status(id);
            const QString statusState = statusMap.value(QStringLiteral("state"), QStringLiteral("unknown")).toString();
            if (statusState != QStringLiteral("unknown")) {
                item.status = statusState;
            }
        }

        items.append(item);
    }

    return items;
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

ProviderDescriptorBuildInput buildProviderDescriptorInput(
    const QString& providerId,
    const ProviderCatalogSnapshot* catalog,
    SettingsStore* settingsStore,
    const ProviderUIService::SecretStatusAccessor& secretStatusAccessor,
    const ProviderUIService::StatusURLAccessor& statusURLAccessor)
{
    ProviderDescriptorBuildInput input;
    input.providerId = providerId;

    const auto catalogEntry = catalog ? catalog->provider(providerId) : std::nullopt;
    if (catalogEntry.has_value()) {
        input.hasDescriptor = catalogEntry->hasDescriptor;
        input.descriptor = catalogEntry->descriptor;
        input.enabled = catalogEntry->enabled;
        input.brandColor = catalogEntry->brandColor;
    }

    if (statusURLAccessor) {
        input.statusURL = statusURLAccessor(providerId);
    }

    input.defaultTokenAccount = TokenAccountStore::instance()->defaultAccountId(providerId);
    input.tokenAccountCount = TokenAccountStore::instance()->accountCountForProvider(providerId);

    if (catalogEntry.has_value()) {
        for (const auto& d : catalogEntry->settingsDescriptors) {
            ProviderSettingFieldBuildInput field;
            field.descriptor = d;
            field.value = d.sensitive
                ? QVariant()
                : (settingsStore ? settingsStore->providerSetting(providerId, d.key, d.defaultValue)
                                 : d.defaultValue);
            if (d.sensitive && secretStatusAccessor) {
                field.secretStatus = secretStatusAccessor(providerId, d.key);
            }
            input.settingsFields.append(field);
        }
    }

    return input;
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

    const QVector<ProviderListBuildItem> items =
        collectProviderListBuildItems(m_catalog, m_snapshotAccessor, m_statusManager);

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

    const ProviderDescriptorBuildInput input = buildProviderDescriptorInput(
        providerId, m_catalog, m_settingsStore, m_secretStatusAccessor, m_statusURLAccessor);

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

void ProviderUIService::invalidateSnapshotDataCache(const QString& providerId)
{
    if (providerId.isEmpty()) {
        m_snapshotDataCache.clear();
    } else {
        m_snapshotDataCache.remove(providerId);
    }
}

void ProviderUIService::invalidateAllCaches()
{
    m_providerListCacheValid = false;
    m_providerListCache.clear();
    m_descriptorCache.clear();
    m_snapshotDataCache.clear();
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

void ProviderUIService::setCodexSnapshotContextAccessor(CodexSnapshotContextAccessor accessor)
{
    m_codexSnapshotContextAccessor = std::move(accessor);
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

QVariantMap ProviderUIService::snapshotData(const QString& id, const UsageSnapshot& snap) const
{
    auto cacheIt = m_snapshotDataCache.find(id);
    if (cacheIt != m_snapshotDataCache.end()) {
        return cacheIt.value();
    }

    const ProviderDescriptor* descriptor = nullptr;
    if (m_catalog) {
        const auto catalogEntry = m_catalog->provider(id);
        if (catalogEntry.has_value() && catalogEntry->hasDescriptor) {
            descriptor = &catalogEntry->descriptor;
        }
    }

    QVariantMap m;
    const bool showUsedPercent = m_settingsStore ? m_settingsStore->usageBarsShowUsed() : false;
    const bool showAbsoluteResetTimes = m_settingsStore ? m_settingsStore->resetTimesShowAbsolute() : false;
    const bool showOptionalFields = m_settingsStore ? m_settingsStore->showOptionalCreditsAndExtraUsage() : true;

    auto resetDisplay = [&](const RateWindow& rw) -> QString {
        if (showAbsoluteResetTimes && rw.resetsAt.has_value() && rw.resetsAt->isValid()) {
            return rw.resetsAt->toLocalTime().toString("yyyy-MM-dd hh:mm");
        }
        return rw.resetDescription.value_or(QString());
    };

    auto addWindowFields = [&](const QString& prefix, const RateWindow& rw, bool isDetailProvider) {
        const double remaining = rw.remainingPercent();
        m[prefix + "Used"] = rw.usedPercent;
        m[prefix + "Remaining"] = remaining;
        m[prefix + "DisplayPercent"] = showUsedPercent ? rw.usedPercent : remaining;
        m[prefix + "DisplayIsUsed"] = showUsedPercent;
        if (rw.resetsAt.has_value()) {
            m[prefix + "ResetsAt"] = rw.resetsAt->toMSecsSinceEpoch();
        }

        if (isDetailProvider && rw.resetDescription.has_value()) {
            QString detail = rw.resetDescription.value().trimmed();
            if (!detail.isEmpty()) {
                m[prefix + "Detail"] = detail;
            }
        } else {
            const QString resetText = resetDisplay(rw);
            if (!resetText.isEmpty()) {
                m[prefix + "ResetDesc"] = resetText;
            }
        }
    };

    m["sessionLabel"] = Localization::providerLabel(descriptor ? descriptor->metadata.sessionLabel : QStringLiteral("Session"));
    m["weeklyLabel"] = Localization::providerLabel(descriptor ? descriptor->metadata.weeklyLabel : QStringLiteral("Weekly"));
    m["opusLabel"] = Localization::providerLabel(
        descriptor && descriptor->metadata.opusLabel.has_value()
            ? descriptor->metadata.opusLabel.value()
            : QString());
    m["supportsCredits"] = descriptor ? descriptor->metadata.supportsCredits : false;
    m["displayName"] = m_displayNameAccessor ? m_displayNameAccessor(id) : id;

    const bool isDetailProvider = (id == "deepseek" || id == "warp" || id == "kilo" ||
                                   id == "abacus" || id == "codebuff");

    if (snap.primary.has_value()) {
        addWindowFields("primary", *snap.primary, isDetailProvider);
    } else {
        m["primaryUsed"] = 0.0;
        m["primaryRemaining"] = 100.0;
        m["primaryDisplayPercent"] = showUsedPercent ? 0.0 : 100.0;
        m["primaryDisplayIsUsed"] = showUsedPercent;
    }
    if (snap.secondary.has_value()) {
        addWindowFields("secondary", *snap.secondary, false);
        m["hasSecondary"] = true;
    } else {
        m["secondaryUsed"] = 0.0;
        m["secondaryRemaining"] = 100.0;
        m["secondaryDisplayPercent"] = showUsedPercent ? 0.0 : 100.0;
        m["secondaryDisplayIsUsed"] = showUsedPercent;
        m["hasSecondary"] = false;
    }
    if (snap.tertiary.has_value()) {
        addWindowFields("tertiary", *snap.tertiary, false);
        m["hasTertiary"] = true;
    } else {
        m["hasTertiary"] = false;
    }
    if (snap.identity.has_value() && snap.identity->loginMethod.has_value()) {
        m["loginMethod"] = snap.identity->loginMethod.value();
    }

    m["hasUsage"] = snap.primary.has_value() || snap.secondary.has_value() || snap.tertiary.has_value();

    if (showOptionalFields && snap.providerCost.has_value()) {
        m["providerCostUsed"] = snap.providerCost->used;
        m["providerCostLimit"] = snap.providerCost->limit;
        m["providerCostCurrency"] = snap.providerCost->currencyCode;
        m["hasProviderCost"] = true;
    } else {
        m["hasProviderCost"] = false;
    }

    m["updatedAt"] = snap.updatedAt.toMSecsSinceEpoch();

    if (showOptionalFields && snap.zaiUsage.has_value()) {
        QVariantMap zai;
        const auto& z = *snap.zaiUsage;
        if (z.tokenLimit.has_value()) {
            QVariantMap tl;
            tl["usedPercent"] = z.tokenLimit->usedPercent();
            tl["windowDescription"] = z.tokenLimit->windowDescription();
            tl["windowLabel"] = z.tokenLimit->windowLabel();
            if (z.tokenLimit->usage.has_value()) tl["usage"] = *z.tokenLimit->usage;
            if (z.tokenLimit->currentValue.has_value()) tl["currentValue"] = *z.tokenLimit->currentValue;
            if (z.tokenLimit->remaining.has_value()) tl["remaining"] = *z.tokenLimit->remaining;
            QVariantList details;
            for (auto& d : z.tokenLimit->usageDetails) {
                QVariantMap dm;
                dm["modelCode"] = d.modelCode;
                dm["usage"] = d.usage;
                details.append(dm);
            }
            tl["usageDetails"] = details;
            zai["tokenLimit"] = tl;
        }
        if (z.timeLimit.has_value()) {
            QVariantMap tl;
            tl["usedPercent"] = z.timeLimit->usedPercent();
            tl["windowDescription"] = z.timeLimit->windowDescription();
            tl["windowLabel"] = z.timeLimit->windowLabel();
            QVariantList details;
            for (auto& d : z.timeLimit->usageDetails) {
                QVariantMap dm;
                dm["modelCode"] = d.modelCode;
                dm["usage"] = d.usage;
                details.append(dm);
            }
            tl["usageDetails"] = details;
            zai["timeLimit"] = tl;
        }
        if (z.sessionTokenLimit.has_value()) {
            QVariantMap sl;
            sl["usedPercent"] = z.sessionTokenLimit->usedPercent();
            sl["windowDescription"] = z.sessionTokenLimit->windowDescription();
            sl["windowLabel"] = z.sessionTokenLimit->windowLabel();
            zai["sessionTokenLimit"] = sl;
        }
        if (z.planName.has_value()) zai["planName"] = *z.planName;
        m["zaiUsage"] = zai;
    }

    if (showOptionalFields && snap.openRouterUsage.has_value()) {
        QVariantMap oru;
        const auto& o = *snap.openRouterUsage;
        oru["totalCredits"] = o.totalCredits;
        oru["totalUsage"] = o.totalUsage;
        oru["balance"] = o.balance;
        oru["usedPercent"] = o.usedPercent;
        oru["keyQuotaStatus"] = static_cast<int>(o.keyQuotaStatus());
        if (o.keyLimit.has_value()) oru["keyLimit"] = *o.keyLimit;
        if (o.keyUsage.has_value()) oru["keyUsage"] = *o.keyUsage;
        if (o.hasValidKeyQuota()) {
            oru["keyRemaining"] = o.keyRemaining();
            oru["keyUsedPercent"] = o.keyUsedPercent();
        }
        if (o.rateLimit.has_value()) {
            QVariantMap rl;
            rl["requests"] = o.rateLimit->requests;
            rl["interval"] = o.rateLimit->interval;
            oru["rateLimit"] = rl;
        }
        m["openRouterUsage"] = oru;
    }

    if (showOptionalFields && snap.providerCost.has_value()) {
        QVariantMap pc;
        pc["used"] = snap.providerCost->used;
        pc["limit"] = snap.providerCost->limit;
        pc["currencyCode"] = snap.providerCost->currencyCode;
        if (snap.providerCost->period.has_value()) pc["period"] = *snap.providerCost->period;
        m["providerCost"] = pc;
    }

    auto addPaceFields = [&](const QString& prefix, const RateWindow& rw) {
        auto pace = UsagePace::weekly(rw, snap.updatedAt);
        if (!pace.has_value()) return;
        auto detail = UsagePaceText::weeklyDetail(*pace);
        m[prefix + "PacePercent"] = pace->expectedUsedPercent;
        m[prefix + "PaceOnTop"] = pace->actualUsedPercent <= pace->expectedUsedPercent;
        m[prefix + "PaceLeftLabel"] = detail.leftLabel;
        m[prefix + "PaceRightLabel"] = detail.rightLabel;
        m[prefix + "PaceStage"] = static_cast<int>(pace->stage);
    };

    if (snap.primary.has_value()) addPaceFields("primary", *snap.primary);
    if (snap.secondary.has_value()) addPaceFields("secondary", *snap.secondary);

    const QString rawError = m_errorAccessor ? m_errorAccessor(id) : QString();
    m["error"] = Localization::providerError(rawError);

    if (id == "codex") {
        CodexConsumerProjection::Context ctx;
        ctx.snapshot = snap;
        ctx.rawUsageError = rawError;
        ctx.now = QDateTime::currentDateTime();

        const CodexSnapshotContext codexContext = m_codexSnapshotContextAccessor
            ? m_codexSnapshotContextAccessor()
            : CodexSnapshotContext{};
        if (codexContext.credits.has_value()) {
            ctx.credits = &codexContext.credits.value();
            ctx.rawCreditsError = codexContext.rawCreditsError;
        }

        auto projection = CodexConsumerProjection::make(
            CodexConsumerProjection::Surface::LiveCard, ctx);

        auto sessionWindow = CodexConsumerProjection::rateWindow(projection, CodexConsumerProjection::RateLane::Session);
        if (sessionWindow.has_value()) {
            addWindowFields("primary", *sessionWindow, false);
        }
        auto weeklyWindow = CodexConsumerProjection::rateWindow(projection, CodexConsumerProjection::RateLane::Weekly);
        if (weeklyWindow.has_value()) {
            addWindowFields("secondary", *weeklyWindow, false);
            m["hasSecondary"] = true;
        }

        m["dashboardVisibility"] = static_cast<int>(projection.dashboardVisibility);
        m["menuBarFallback"] = static_cast<int>(projection.menuBarFallback);

        if (!projection.userFacingErrors.usage.isEmpty()) {
            m["error"] = projection.userFacingErrors.usage;
        }

        if (projection.credits.has_value() && projection.credits->snapshot.has_value()) {
            m["hasCredits"] = true;
            m["creditsRemaining"] = projection.credits->snapshot->remaining;
            if (!projection.credits->userFacingError.isEmpty()) {
                m["creditsError"] = projection.credits->userFacingError;
            }
        } else if (snap.providerCost.has_value()) {
            m["hasCredits"] = true;
            m["creditsRemaining"] = snap.providerCost->used;
        } else {
            m["hasCredits"] = false;
        }

        if (snap.providerCost.has_value() &&
            snap.providerCost->period.has_value() &&
            snap.providerCost->period.value() == "Credits") {
            m["hasProviderCost"] = false;
        }

        m["hasExhaustedRateLane"] = CodexConsumerProjection::hasExhaustedRateLane(projection);
    }

    m_snapshotDataCache[id] = m;
    return m;
}

QVariantList ProviderUIService::buildProviderListNow() const
{
    if (!m_catalog) {
        return {};
    }

    const QVector<ProviderListBuildItem> items =
        collectProviderListBuildItems(m_catalog, m_snapshotAccessor, m_statusManager);

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

    if (!m_catalog->provider(id).has_value()) {
        return {};
    }

    const ProviderDescriptorBuildInput input = buildProviderDescriptorInput(
        id, m_catalog, m_settingsStore, m_secretStatusAccessor, m_statusURLAccessor);

    const QVariantMap data = buildProviderDescriptorFromInput(input);
    m_descriptorCache.insert(id, data);
    return data;
}
