#include "UsageStore.h"
#include "BatchUpdateController.h"
#include "CostUsageService.h"
#include "Localization.h"
#include "PlanUtilizationHistoryStore.h"
#include "SessionQuotaNotifications.h"
#include "UsageBackend.h"
#include "UsageBackendJobs.h"
#include "UsageBackendTypes.h"
#include "../providers/codex/CodexActiveSource.h"
#include "../providers/ProviderRegistry.h"
#include "../providers/ProviderPipeline.h"
#include "../providers/ProviderFetchContext.h"
#include "../providers/shared/ProviderCredentialStore.h"
#include "../providers/ProviderCredentialManager.h"
#include "../providers/ProviderStatusManager.h"
#include "../providers/ProviderConnectionTester.h"
#include "../providers/ProviderLoginManager.h"
#include "../account/TokenAccountOperationManager.h"
#include "ProviderUIService.h"
#include "ProviderRefreshCoordinator.h"
#include "../providers/shared/ProviderStatusFetcher.h"
#include "../app/SettingsStore.h"
#include "../network/NetworkManager.h"
#include "../util/UsagePaceText.h"
#include "../models/UsagePace.h"
#include "../providers/codex/ManagedCodexAccountService.h"
#include "../providers/codex/CodexCreditsFetcher.h"
#include "../providers/codex/CodexDashboardCache.h"
#include "../account/TokenAccountStore.h"
#include "../runtime/ProviderRuntimeManager.h"

#include <QDateTime>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QPointer>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QElapsedTimer>

#include <memory>

// Performance probe: logs when a scoped block exceeds thresholdMicros.
// Only active in debug builds to avoid overhead in production.
#ifdef QT_DEBUG
struct PerfProbe {
    QElapsedTimer timer;
    const char* name;
    qint64 thresholdMicros;
    PerfProbe(const char* n, qint64 t) : name(n), thresholdMicros(t) { timer.start(); }
    ~PerfProbe() {
        qint64 elapsed = timer.nsecsElapsed() / 1000;
        if (elapsed >= thresholdMicros) {
            qDebug() << "[PERF]" << name << "took" << elapsed << "us";
        }
    }
};
#define PERF_PROBE(name, thresholdMicros) PerfProbe _perfProbe(name, thresholdMicros);
#else
#define PERF_PROBE(name, thresholdMicros)
#endif

namespace {

// Lazily-populated snapshot of system environment variables.
// Populated once on first access and then cached for the process lifetime.
// This avoids repeated QProcessEnvironment::systemEnvironment() calls which
// invoke GetEnvironmentStrings() on Windows each time.
// After initial population, any env vars set via qputenv() after cache
// creation are NOT reflected. Call rebuildSystemEnvCache() to force a refresh.
QHash<QString, QString> g_systemEnvCache;
bool g_systemEnvCachePopulated = false;

const QHash<QString, QString>& cachedSystemEnv()
{
    if (!g_systemEnvCachePopulated) {
        auto systemEnv = QProcessEnvironment::systemEnvironment();
        const auto keys = systemEnv.keys();
        for (const auto& key : keys) {
            g_systemEnvCache.insert(key, systemEnv.value(key));
        }
        g_systemEnvCachePopulated = true;
    }
    return g_systemEnvCache;
}

void rebuildSystemEnvCache()
{
    g_systemEnvCache.clear();
    auto systemEnv = QProcessEnvironment::systemEnvironment();
    const auto keys = systemEnv.keys();
    for (const auto& key : keys) {
        g_systemEnvCache.insert(key, systemEnv.value(key));
    }
    g_systemEnvCachePopulated = true;
}

} // namespace

UsageStore::UsageStore(QObject* parent)
    : QObject(parent)
    , m_backend(new UsageBackend(this))
    , m_pipeline(new ProviderPipeline(this))
    , m_historyStore(new PlanUtilizationHistoryStore(this))
{
    rebuildProviderCatalogSnapshot();
    connect(m_backend, &UsageBackend::jobFinished,
            this, &UsageStore::handleBackendResult);

    // Initialize batch update controller to avoid signal storm
    m_batchUpdater = new BatchUpdateController(this);
    connect(m_batchUpdater, &BatchUpdateController::batchUpdateReady,
            this, &UsageStore::onBatchUpdateReady);
    connect(m_batchUpdater, &BatchUpdateController::batchFinished,
            this, &UsageStore::onBatchFinished);

    QObject::connect(&m_statusTimer, &QTimer::timeout, this, &UsageStore::refreshProviderStatuses);
    QObject::connect(m_pipeline, &ProviderPipeline::pipelineComplete,
                     this, [this](const ProviderFetchResult& /*result*/) {
    });

    // Initialize Codex multi-account service
    m_codexAccountService = new ManagedCodexAccountService(cachedSystemEnv(), this);
    m_codexAccountService->setBackend(m_backend);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::accountsChanged,
                     this, &UsageStore::codexAccountsChanged);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::accountsChanged,
                     this, &UsageStore::codexAccountStateChanged);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::activeAccountChanged,
                     this, &UsageStore::codexActiveAccountChanged);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::activeAccountChanged,
                     this, [this](const QString&) {
        emit codexAccountStateChanged();
    });
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::authenticationStarted,
                     this, &UsageStore::codexAuthenticationStarted);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::authenticationStarted,
                     this, [this](const QString&) { emit codexAccountStateChanged(); });
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::authenticationFinished,
                     this, &UsageStore::codexAuthenticationFinished);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::authenticationFinished,
                     this, [this](const QString&, bool success) {
        emit codexAccountStateChanged();
        Q_UNUSED(success)
    });
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::authenticationStateChanged,
                     this, &UsageStore::codexAccountStateChanged);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::removalStarted,
                     this, &UsageStore::codexRemovalStarted);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::removalStarted,
                     this, [this](const QString&) { emit codexAccountStateChanged(); });
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::removalFinished,
                     this, &UsageStore::codexRemovalFinished);
    QObject::connect(m_codexAccountService, &ManagedCodexAccountService::removalFinished,
                     this, [this](const QString&, bool) { emit codexAccountStateChanged(); });

    // Initialize credential manager (Phase 1 extraction)
    m_credentialManager = new ProviderCredentialManager(this);
    QObject::connect(m_credentialManager, &ProviderCredentialManager::secretChanged,
                     this, &UsageStore::providerSecretChanged);

    // Initialize status manager (Phase 2 extraction)
    m_statusManager = new ProviderStatusManager(this);
    QObject::connect(m_statusManager, &ProviderStatusManager::statusChanged,
                     this, &UsageStore::providerStatusChanged);
    QObject::connect(m_statusManager, &ProviderStatusManager::revisionChanged,
                     this, [this]() {
        m_uiService->invalidateProviderListCache();
        emit statusRevisionChanged();
    });

    // Initialize connection tester (Phase 3 extraction)
    m_connectionTester = new ProviderConnectionTester(this);
    QObject::connect(m_connectionTester, &ProviderConnectionTester::testStateChanged,
                     this, &UsageStore::providerConnectionTestChanged);

    // Initialize login manager (Phase 3 extraction)
    m_loginManager = new ProviderLoginManager(this);
    QObject::connect(m_loginManager, &ProviderLoginManager::loginStateChanged,
                     this, &UsageStore::providerLoginStateChanged);

    // Initialize token account operation manager (Phase 4 extraction)
    m_tokenAccountManager = new TokenAccountOperationManager(this);
    QObject::connect(m_tokenAccountManager, &TokenAccountOperationManager::operationStateChanged,
                     this, &UsageStore::tokenAccountOperationStateChanged);
    QObject::connect(m_tokenAccountManager, &TokenAccountOperationManager::operationFinished,
                     this, &UsageStore::tokenAccountOperationFinished);
    QObject::connect(m_tokenAccountManager, &TokenAccountOperationManager::refreshProviderRequested,
                     this, &UsageStore::refreshProvider);

    // Initialize UI service (Phase 5 extraction)
    m_uiService = new ProviderUIService(this);
    m_uiService->setCatalog(&m_providerCatalog);
    m_uiService->setStatusManager(m_statusManager);
    m_uiService->setCredentialManager(m_credentialManager);
    m_uiService->setBackend(m_backend);
    m_uiService->setSnapshotAccessor([this](const QString& providerId) -> std::optional<double> {
        UsageSnapshot snap = m_refreshCoordinator->snapshot(providerId);
        if (snap.primary.has_value()) {
            return snap.primary->usedPercent;
        }
        return std::nullopt;
    });
    m_uiService->setSecretStatusAccessor([this](const QString& providerId, const QString& key) -> QVariantMap {
        return providerSecretStatus(providerId, key);
    });
    m_uiService->setErrorAccessor([this](const QString& providerId) -> QString {
        return m_refreshCoordinator ? m_refreshCoordinator->error(providerId) : QString();
    });
    m_uiService->setDisplayNameAccessor([this](const QString& providerId) -> QString {
        return providerDisplayName(providerId);
    });
    m_uiService->setStatusURLAccessor([this](const QString& providerId) -> QString {
        return providerStatusURL(providerId);
    });
    m_uiService->setCodexSnapshotContextAccessor([this]() -> ProviderUIService::CodexSnapshotContext {
        ProviderUIService::CodexSnapshotContext context;
        if (m_codexCreditsCache.snapshot.has_value() &&
            m_codexCreditsCache.accountKey == currentCodexAccountKey()) {
            context.credits = m_codexCreditsCache.snapshot;
            context.rawCreditsError = m_codexCreditsCache.lastError;
        }
        return context;
    });
    QObject::connect(m_uiService, &ProviderUIService::providerListModelChanged,
                     this, &UsageStore::providerListModelChanged);
    QObject::connect(m_uiService, &ProviderUIService::providerDescriptorChanged,
                     this, &UsageStore::providerDescriptorChanged);

    // Initialize refresh coordinator (Phase 6 extraction)
    m_refreshCoordinator = new ProviderRefreshCoordinator(this);
    m_refreshCoordinator->setBackend(m_backend);
    m_refreshCoordinator->setFetchCommandInputBuilder(
        [this](const QString& providerId) -> UsageBackendJobs::ProviderFetchCommandInput {
            return buildProviderFetchCommandInput(providerId);
        });
    m_refreshCoordinator->setProviderResolver(
        [](const QString& providerId) -> IProvider* {
            return ProviderRegistry::instance().provider(providerId);
        });

    // Connect auto-refresh timer to refresh
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::autoRefreshTriggered,
                     this, &UsageStore::refresh);

    // Forward coordinator signals
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::snapshotChanged,
                     this, [this](const QString& providerId) {
        m_uiService->invalidateSnapshotDataCache(providerId);
        if (m_batchInProgress && m_batchUpdater) {
            m_batchUpdater->markDirty(providerId);
        } else {
            emit snapshotChanged(providerId);
        }
    });
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::revisionChanged,
                     this, [this]() {
        m_uiService->invalidateSnapshotDataCache(QString());
        emit snapshotRevisionChanged();
    });
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::refreshingChanged,
                     this, &UsageStore::refreshingChanged);
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::errorOccurred,
                     this, &UsageStore::errorOccurred);
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::credentialCacheUpdatesReady,
                     this, &UsageStore::applyCredentialCacheUpdates);
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::fetchAttemptsChanged,
                     this, [this](const QString& providerId) {
        if (providerId == QLatin1String("codex")) {
            emit codexFetchAttemptsChanged();
        }
    });
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::providerRefreshSuccess,
                     this, &UsageStore::onProviderRefreshSuccess);
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::providerRefreshFailed,
                     this, &UsageStore::onProviderRefreshFailed);
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::refreshStarted,
                     this, [this](const QStringList& ids) {
        if (ids.size() > 1 && m_batchUpdater) {
            m_batchInProgress = true;
            m_batchUpdater->beginBatch();
        }
    });
    QObject::connect(m_refreshCoordinator, &ProviderRefreshCoordinator::refreshComplete,
                     this, [this]() {
        if (m_batchInProgress && m_batchUpdater) {
            m_batchInProgress = false;
            m_batchUpdater->endBatch();
        }
    });

    TokenAccountStore* tokenStore = TokenAccountStore::instance();
    QObject::connect(tokenStore, &TokenAccountStore::accountsChanged,
                     this, [this](const QString& providerId) {
        m_uiService->invalidateProviderListCache();
        m_uiService->invalidateDescriptorCache(providerId);
        emit tokenAccountsChanged(providerId);
    });
    QObject::connect(tokenStore, &TokenAccountStore::defaultAccountChanged,
                     this, [this](const QString& providerId, const QString&) {
        m_uiService->invalidateProviderListCache();
        m_uiService->invalidateDescriptorCache(providerId);
        emit tokenAccountsChanged(providerId);
    });
    QObject::connect(this, &UsageStore::providerSecretChanged,
                     this, [this](const QString& providerId, const QString&) {
        m_uiService->invalidateDescriptorCache(providerId);
    });
}

void UsageStore::setSettingsStore(SettingsStore* s) {
    if (m_settingsStore == s) return;
    if (m_settingsStore) {
        disconnect(m_settingsStore, nullptr, this, nullptr);
    }
    m_settingsStore = s;
    if (m_uiService) {
        m_uiService->setSettingsStore(s);
    }
    if (m_settingsStore) {
        connect(m_settingsStore, &SettingsStore::statusChecksEnabledChanged,
                this, &UsageStore::configureStatusPolling);
        auto notifyDisplaySettingsChanged = [this]() {
            m_uiService->invalidateSnapshotDataCache(QString());
            emit snapshotRevisionChanged();
            // snapshotRevisionChanged() is sufficient; TrayPanel/PlanUtilizationChart
            // bind to snapshotRevision. No need to emit snapshotChanged(id) individually.
        };
        connect(m_settingsStore, &SettingsStore::usageBarsShowUsedChanged,
                this, notifyDisplaySettingsChanged);
        connect(m_settingsStore, &SettingsStore::resetTimesShowAbsoluteChanged,
                this, notifyDisplaySettingsChanged);
        connect(m_settingsStore, &SettingsStore::showOptionalCreditsAndExtraUsageChanged,
                this, notifyDisplaySettingsChanged);
    }
    configureStatusPolling();
}

void UsageStore::configureStatusPolling() {
    const bool enabled = m_settingsStore ? m_settingsStore->statusChecksEnabled() : true;
    if (!enabled) {
        m_statusTimer.stop();
        return;
    }
    m_statusTimer.start(5 * 60 * 1000);
}

UsageSnapshot UsageStore::snapshot(const QString& providerId) const {
    return m_refreshCoordinator ? m_refreshCoordinator->snapshot(providerId) : UsageSnapshot{};
}

bool UsageStore::isProviderEnabled(const QString& id) const {
    if (auto entry = m_providerCatalog.provider(id); entry.has_value()) {
        return entry->enabled;
    }
    return false;
}

void UsageStore::setProviderEnabled(const QString& id, bool enabled) {
    ProviderRegistry::instance().setProviderEnabled(id, enabled);
    ProviderRuntimeManager::instance()->setProviderRuntimeEnabled(id, enabled);
    if (m_settingsStore) {
        m_settingsStore->setProviderEnabled(id, enabled);
    }
    m_uiService->invalidateDescriptorCache(id);
    updateProviderIDs();
    emit providerDescriptorChanged(id);
}

QString UsageStore::providerDisplayName(const QString& id) const {
    if (auto entry = m_providerCatalog.provider(id); entry.has_value() && entry->hasDescriptor) {
        return entry->descriptor.metadata.displayName;
    }
    return id;
}

bool UsageStore::isRefreshing() const
{
    return m_refreshCoordinator ? m_refreshCoordinator->isRefreshing() : false;
}

int UsageStore::snapshotRevision() const
{
    return m_refreshCoordinator ? m_refreshCoordinator->revision() : 0;
}

int UsageStore::statusRevision() const
{
    return m_statusManager ? m_statusManager->revision() : 0;
}

QStringList UsageStore::providerIDs() const {
    return m_providerIDs;
}

void UsageStore::rebuildSystemEnvCache()
{
    ::rebuildSystemEnvCache();
}

void UsageStore::preloadCredentials() {
    const CredentialPreloadPayload payload =
        UsageBackendJobs::preloadCredentials(buildCredentialPreloadItems());
    applyCredentialCacheUpdates(payload.updates);
}

void UsageStore::requestPreloadCredentials()
{
    const QVector<UsageBackendJobs::CredentialPreloadItem> items = buildCredentialPreloadItems();
    if (items.isEmpty()) {
        return;
    }

    m_backend->dispatchValueJob(QStringLiteral("credentialPreload"), 0,
                                [items]() -> QVariant {
        return QVariant::fromValue(UsageBackendJobs::preloadCredentials(items));
    });
}

QVector<UsageBackendJobs::CredentialPreloadItem> UsageStore::buildCredentialPreloadItems() const
{
    QVector<UsageBackendJobs::CredentialPreloadItem> items;
    for (const auto& provider : m_providerCatalog.providers()) {
        for (const auto& desc : provider.settingsDescriptors) {
            if (!desc.sensitive || desc.credentialTarget.isEmpty()) continue;
            {
                QMutexLocker locker(&m_credentialCacheMutex);
                if (m_credentialCache.contains(desc.credentialTarget)) continue;
                if (m_credentialMissing.contains(desc.credentialTarget)) continue;
            }
            UsageBackendJobs::CredentialPreloadItem item;
            item.providerId = provider.id;
            item.key = desc.key;
            item.target = desc.credentialTarget;
            items.append(item);
        }
    }
    return items;
}

void UsageStore::applyCredentialCacheUpdates(const QVector<CredentialCacheUpdatePayload>& updates)
{
    if (updates.isEmpty()) {
        return;
    }

    QMutexLocker locker(&m_credentialCacheMutex);
    for (const auto& update : updates) {
        if (update.target.isEmpty()) {
            continue;
        }
        if (update.exists) {
            m_credentialCache[update.target] = {update.data, QDateTime::currentDateTime()};
            m_credentialExisting.insert(update.target);
            m_credentialMissing.remove(update.target);
        } else {
            m_credentialCache.remove(update.target);
            m_credentialExisting.remove(update.target);
            m_credentialMissing[update.target] = true;
        }
        m_credentialStatusInFlight.remove(update.target);
    }
}

UsageBackendJobs::ProviderFetchCommandInput
UsageStore::buildProviderFetchCommandInput(const QString& providerId) const
{
    UsageBackendJobs::ProviderFetchCommandInput input;
    input.providerId = providerId;
    input.env = cachedSystemEnv();

    auto addProviderSetting = [&](const QString& key, const QVariant& defaultValue = QVariant()) {
        const QVariant value = m_settingsStore
            ? m_settingsStore->providerSetting(providerId, key, defaultValue)
            : defaultValue;
        input.providerSettings.insert(key, value);
        return value;
    };

    if (const auto entry = m_providerCatalog.provider(providerId); entry.has_value()) {
        for (const auto& descriptor : entry->settingsDescriptors) {
            UsageBackendJobs::ProviderFetchSettingInput field;
            field.descriptor = descriptor;
            field.value = addProviderSetting(descriptor.key, descriptor.defaultValue);
            input.settingsFields.append(field);

            if (descriptor.sensitive && !descriptor.credentialTarget.isEmpty()) {
                UsageBackendJobs::CredentialCacheInput cache;
                cache.target = descriptor.credentialTarget;
                {
                    QMutexLocker locker(&m_credentialCacheMutex);
                    auto cacheIt = m_credentialCache.find(descriptor.credentialTarget);
                    if (cacheIt != m_credentialCache.end()
                        && cacheIt.value().cachedAt.msecsTo(QDateTime::currentDateTime()) < CREDENTIAL_CACHE_TTL_MS) {
                        cache.hasValue = true;
                        cache.value = cacheIt.value().data;
                    }
                    cache.missing = m_credentialMissing.contains(descriptor.credentialTarget);
                }
                input.credentialCache.insert(descriptor.credentialTarget, cache);
            }
        }
    }

    addProviderSetting(QStringLiteral("sourceMode"), QStringLiteral("auto"));
    addProviderSetting(QStringLiteral("codexDataSource"), QStringLiteral("auto"));
    addProviderSetting(QStringLiteral("claudeDataSource"), QStringLiteral("auto"));
    addProviderSetting(QStringLiteral("cookieSource"), QStringLiteral("auto"));
    addProviderSetting(QStringLiteral("cursorCookieSource"), QStringLiteral("auto"));
    addProviderSetting(QStringLiteral("manualCookieHeader"), QString());
    addProviderSetting(QStringLiteral("accountID"), QString());
    addProviderSetting(QStringLiteral("networkTimeoutMs"), ProviderPipeline::STRATEGY_TIMEOUT_MS);
    addProviderSetting(QStringLiteral("apiRegion"), QStringLiteral("global"));

    if (providerId == QLatin1String("codex") && m_codexAccountService) {
        input.codexActiveAccountId = m_codexAccountService->activeAccountID();
        input.codexManagedHomePath = m_codexAccountService->activeManagedHomePath();
    }
    input.defaultTokenAccountId = TokenAccountStore::instance()->defaultAccountId(providerId);
    return input;
}

ProviderFetchContext UsageStore::buildFetchContextForProvider(const QString& providerId) const {
    PERF_PROBE("buildFetchContextForProvider", 2000);
    ProviderFetchContext ctx;
    ctx.providerId = providerId;
    ctx.sourceMode = ProviderSourceMode::Auto;
    ctx.isAppRuntime = true;
    ctx.allowInteractiveAuth = false;
    ctx.networkTimeoutMs = ProviderPipeline::STRATEGY_TIMEOUT_MS;

    const auto& env = cachedSystemEnv();
    for (auto it = env.constBegin(); it != env.constEnd(); ++it) {
        ctx.env[it.key()] = it.value();
    }

    auto addSetting = [&](const QString& key, const QVariant& defaultValue = QVariant()) {
        QVariant value = m_settingsStore
            ? m_settingsStore->providerSetting(providerId, key, defaultValue)
            : defaultValue;
        if (value.isValid()) {
            ctx.settings.set(key, value);
        }
        return value;
    };

    if (const auto entry = m_providerCatalog.provider(providerId); entry.has_value()) {
        for (const auto& descriptor : entry->settingsDescriptors) {
            if (descriptor.sensitive) {
                QString secret;
                if (!descriptor.envVar.isEmpty() && ctx.env.contains(descriptor.envVar)) {
                    secret = ctx.env.value(descriptor.envVar).trimmed();
                }
                if (secret.isEmpty() && !descriptor.credentialTarget.isEmpty()) {
                    // Use cached credential to avoid blocking main thread with WinCred API
                    bool cacheHit = false;
                    bool cacheExpired = true;
                    bool isMissing = false;
                    {
                        QMutexLocker locker(&m_credentialCacheMutex);
                        auto cacheIt = m_credentialCache.find(descriptor.credentialTarget);
                        if (cacheIt != m_credentialCache.end()) {
                            cacheHit = true;
                            cacheExpired = cacheIt.value().cachedAt.msecsTo(QDateTime::currentDateTime()) >= CREDENTIAL_CACHE_TTL_MS;
                            if (!cacheExpired) {
                                secret = QString::fromUtf8(cacheIt.value().data).trimmed();
                            }
                        }
                        isMissing = m_credentialMissing.contains(descriptor.credentialTarget);
                    }
                    if (!cacheHit && !isMissing) {
                        auto stored = ProviderCredentialStore::read(descriptor.credentialTarget);
                        {
                            QMutexLocker locker(&m_credentialCacheMutex);
                            if (stored.has_value()) {
                                secret = QString::fromUtf8(stored.value()).trimmed();
                                m_credentialCache[descriptor.credentialTarget] = {stored.value(), QDateTime::currentDateTime()};
                            } else {
                                m_credentialMissing[descriptor.credentialTarget] = true;
                            }
                        }
                    }
                }
                if (secret.isEmpty()) {
                    secret = addSetting(descriptor.key, descriptor.defaultValue).toString().trimmed();
                } else {
                    ctx.settings.set(descriptor.key, secret);
                }
            } else {
                addSetting(descriptor.key, descriptor.defaultValue);
            }
        }
    }

    QString sourceMode = addSetting("sourceMode", "auto").toString();
    if (sourceMode == "auto") {
        if (providerId == "codex") {
            sourceMode = addSetting("codexDataSource", "auto").toString();
        } else if (providerId == "claude") {
            sourceMode = addSetting("claudeDataSource", "auto").toString();
        }
    }
    ctx.settings.set("sourceMode", sourceMode);
    ctx.sourceMode = sourceModeFromString(sourceMode);

    QString cookieSource = addSetting(
        "cookieSource",
        ctx.settings.get("cookieSource", "auto")).toString();
    if (providerId == "cursor" && cookieSource == "auto") {
        cookieSource = addSetting("cursorCookieSource", "auto").toString();
    }
    ctx.settings.set("cookieSource", cookieSource);

    QString manualCookie = ctx.settings.get("manualCookieHeader").toString().trimmed();
    if (manualCookie.isEmpty()) {
        manualCookie = addSetting("manualCookieHeader", "").toString().trimmed();
    }
    if (!manualCookie.isEmpty()) {
        ctx.manualCookieHeader = manualCookie;
    }

    QString accountId = addSetting("accountID", "").toString().trimmed();
    ctx.accountID = accountId;

    if (providerId == "codex" && m_codexAccountService) {
        const QString activeId = m_codexAccountService->activeAccountID();
        if (!activeId.isEmpty()) {
            ctx.accountID = activeId;
        }

        const QString managedHome = m_codexAccountService->activeManagedHomePath();
        if (!managedHome.isEmpty()) {
            ctx.env["CODEX_HOME"] = managedHome;
        }
    }

    bool ok = false;
    int timeout = addSetting("networkTimeoutMs", ProviderPipeline::STRATEGY_TIMEOUT_MS).toInt(&ok);
    if (ok && timeout > 0) {
        ctx.networkTimeoutMs = timeout;
    }

    QString apiRegion = addSetting("apiRegion", "global").toString();
    if (providerId == "zai") {
        ctx.env["ZAI_API_REGION"] = apiRegion;
    }

    // Token Account resolution
    TokenAccountStore* accountStore = TokenAccountStore::instance();
    QString resolvedAccountId = ctx.accountID;
    if (resolvedAccountId.isEmpty()) {
        resolvedAccountId = accountStore->defaultAccountId(providerId);
    }
    if (!resolvedAccountId.isEmpty()) {
        auto accOpt = accountStore->accountWithCredentials(resolvedAccountId);
        if (accOpt.has_value()) {
            const TokenAccount& acc = accOpt.value();
            ctx.accountID = acc.accountId;
            ctx.accountCredentials = acc.credentials;
            // Per-account source mode override (if not Auto)
            if (acc.sourceMode != ProviderSourceMode::Auto) {
                ctx.sourceMode = acc.sourceMode;
            }
        }
    }

    return ctx;
}

void UsageStore::rebuildProviderCatalogSnapshot()
{
    m_providerCatalog = ProviderCatalogSnapshot::fromRegistry(ProviderRegistry::instance(),
                                                              ++m_providerCatalogGeneration);
}

void UsageStore::updateProviderIDs() {
    rebuildProviderCatalogSnapshot();
    m_providerIDs = m_providerCatalog.enabledProviderIDs();
    m_uiService->invalidateProviderListCache();
    emit providerIDsChanged();
}

QVariantMap UsageStore::snapshotData(const QString& id) const {
    PERF_PROBE("snapshotData", 1000);
    return m_uiService->snapshotData(id, snapshot(id));
}

void UsageStore::setCostUsageEnabled(bool v) {
    if (m_costUsageEnabled != v) {
        m_costUsageEnabled = v;
        emit costUsageEnabledChanged();
        if (v) refreshCostUsage();
    }
}

void UsageStore::ensureCostUsageEnabled() {
    if (!m_costUsageEnabled) {
        setCostUsageEnabled(true);
    }
}

void UsageStore::releaseCostUsageViewCaches() const {
    m_costUsageDetailsRowsCache.clear();
    m_costUsageProviderDetailCache.clear();
    m_costUsageProviderDetailQueued.clear();
    m_costUsageDetailsRowsCacheValid = false;
    m_costUsageTokenProviderCountCache = 0;
    m_costUsageDetailsRowsBuildQueued = false;
    ++m_costUsageDetailsRowsBuildGeneration;
    ++m_costUsageProviderDetailBuildGeneration;
}

void UsageStore::resetCostUsageDerivedCaches(bool clearBuiltData)
{
    if (clearBuiltData) {
        m_costUsageDataCache.clear();
        m_providerCostUsageListCache.clear();
        m_costUsageDetailsRowsCache.clear();
    }

    m_costUsageProviderDetailCache.clear();
    m_costUsageProviderDetailQueued.clear();
    m_costUsageDataCacheValid = false;
    m_providerCostUsageListCacheValid = false;
    m_costUsageDetailsRowsCacheValid = false;
    m_costUsageTokenProviderCountCache = 0;
    m_costUsageSummaryBuildQueued = false;
    m_costUsageProviderRowsBuildQueued = false;
    m_costUsageDetailsRowsBuildQueued = false;
    ++m_costUsageSummaryBuildGeneration;
    ++m_costUsageProviderRowsBuildGeneration;
    ++m_costUsageDetailsRowsBuildGeneration;
    ++m_costUsageProviderDetailBuildGeneration;
}

void UsageStore::requestCostUsageViewData()
{
    requestCostUsageSummary();
}

void UsageStore::requestCostUsageSummary() const
{
    if (m_costUsageDataCacheValid || m_costUsageSummaryBuildQueued) {
        return;
    }

    m_costUsageSummaryBuildQueued = true;
    const int generation = ++m_costUsageSummaryBuildGeneration;
    const CostUsageSnapshot costUsage = m_costUsage;

    m_backend->dispatchValueJob(QStringLiteral("costUsageSummary"), generation,
                                [costUsage]() -> QVariant {
        PERF_PROBE("costUsageSummary_worker", 5000);
        CostUsageSummaryPayload payload;
        payload.costData = CostUsageService::summaryData(costUsage);
        return QVariant::fromValue(payload);
    });
}

void UsageStore::requestCostUsageProviderRows() const
{
    if (m_providerCostUsageListCacheValid || m_costUsageProviderRowsBuildQueued) {
        return;
    }

    m_costUsageProviderRowsBuildQueued = true;
    const int generation = ++m_costUsageProviderRowsBuildGeneration;
    const QVector<ProviderCostUsageSnapshot> allProviders = m_allProviderCostUsage;

    m_backend->dispatchValueJob(QStringLiteral("costUsageProviderRows"), generation,
                                [allProviders]() -> QVariant {
        PERF_PROBE("costUsageProviderRows_worker", 5000);
        CostUsageProviderRowsPayload payload;
        payload.providerList = CostUsageService::providerRows(allProviders);
        return QVariant::fromValue(payload);
    });
}

void UsageStore::requestCostUsageDetailsRows() const
{
    if (m_costUsageDetailsRowsCacheValid || m_costUsageDetailsRowsBuildQueued) {
        return;
    }

    if (!m_uiService->isProviderListCacheValid()) {
        QMetaObject::invokeMethod(const_cast<UsageStore*>(this),
                                  &UsageStore::requestProviderList,
                                  Qt::QueuedConnection);
        return;
    }

    m_costUsageDetailsRowsBuildQueued = true;
    const int generation = ++m_costUsageDetailsRowsBuildGeneration;
    const QVector<ProviderCostUsageSnapshot> allProviders = m_allProviderCostUsage;
    const QVariantList appProviders = m_uiService->cachedProviderList();

    m_backend->dispatchValueJob(QStringLiteral("costUsageDetailsRows"), generation,
                                [allProviders, appProviders]() -> QVariant {
        PERF_PROBE("costUsageDetailsRows_worker", 5000);
        return QVariant::fromValue(CostUsageService::detailsRows(allProviders, appProviders));
    });
}

QSet<QString> UsageStore::costUsageSubscribedProviderIDs() const
{
    QSet<QString> providerIds;
    for (const auto& provider : m_providerCatalog.providers()) {
        if (!provider.enabled) {
            continue;
        }
        if (!TokenAccountStore::instance()->visibleAccountsForProvider(provider.id).isEmpty()) {
            providerIds.insert(provider.id);
        }
    }
    return providerIds;
}

void UsageStore::refreshCostUsage() {
    if (!m_costUsageEnabled || m_costUsageRefreshing) return;

    const CostUsageScanPlan plan = CostUsageService::buildScanPlan(
        m_providerCatalog.enabledProviderIDs(), costUsageSubscribedProviderIDs());
    if (!plan.hasWork()) {
        const bool hadData = m_costUsage.last30DaysTokens > 0
            || !m_perProviderCostUsage.isEmpty()
            || !m_allProviderCostUsage.isEmpty()
            || m_costUsageDataCacheValid
            || m_providerCostUsageListCacheValid
            || m_costUsageDetailsRowsCacheValid
            || !m_costUsageProviderDetailCache.isEmpty();
        m_costUsage = CostUsageSnapshot{};
        m_perProviderCostUsage.clear();
        m_allProviderCostUsage.clear();
        resetCostUsageDerivedCaches(true);
        if (hadData) {
            emit costUsageChanged();
        }
        return;
    }

    m_costUsageRefreshing = true;
    emit costUsageRefreshingChanged();
    const int generation = ++m_costUsageRefreshGeneration;

    m_backend->dispatchValueJob(QStringLiteral("costUsageRefresh"), generation,
                                [plan]() -> QVariant {
        PERF_PROBE("refreshCostUsage_worker", 5000);
        return QVariant::fromValue(CostUsageService::refresh(plan));
    });
}

QVariantMap UsageStore::costUsageData() const {
    PERF_PROBE("costUsageData", 1000);
    if (m_costUsageDataCacheValid) {
        return m_costUsageDataCache;
    }
    QMetaObject::invokeMethod(const_cast<UsageStore*>(this),
                              &UsageStore::requestCostUsageSummary,
                              Qt::QueuedConnection);
    return m_costUsageDataCache;
}

QVariantList UsageStore::providerCostUsageList() const {
    PERF_PROBE("providerCostUsageList", 1000);
    if (m_providerCostUsageListCacheValid) {
        return m_providerCostUsageListCache;
    }
    QMetaObject::invokeMethod(const_cast<UsageStore*>(this),
                              &UsageStore::requestCostUsageProviderRows,
                              Qt::QueuedConnection);
    return m_providerCostUsageListCache;
}

QVariantList UsageStore::costUsageDetailsRows() const
{
    PERF_PROBE("costUsageDetailsRows", 1000);
    if (m_costUsageDetailsRowsCacheValid) {
        return m_costUsageDetailsRowsCache;
    }
    QMetaObject::invokeMethod(const_cast<UsageStore*>(this),
                              &UsageStore::requestCostUsageDetailsRows,
                              Qt::QueuedConnection);
    return m_costUsageDetailsRowsCache;
}

int UsageStore::costUsageTokenProviderCount() const
{
    return m_costUsageTokenProviderCountCache;
}

QVariantMap UsageStore::costUsageProviderDetail(const QString& providerId) const
{
    return m_costUsageProviderDetailCache.value(providerId);
}

void UsageStore::requestCostUsageProviderDetail(const QString& providerId) const
{
    if (providerId.isEmpty()
        || m_costUsageProviderDetailCache.contains(providerId)
        || m_costUsageProviderDetailQueued.contains(providerId)) {
        return;
    }

    m_costUsageProviderDetailQueued.insert(providerId);
    const int generation = m_costUsageProviderDetailBuildGeneration;
    const QVector<ProviderCostUsageSnapshot> allProviders = m_allProviderCostUsage;

    m_backend->dispatchValueJob(QStringLiteral("costUsageProviderDetail"), generation,
                                [providerId, allProviders]() -> QVariant {
        PERF_PROBE("costUsageProviderDetail_worker", 5000);
        return QVariant::fromValue(CostUsageService::providerDetail(providerId, allProviders));
    });
}

void UsageStore::handleBackendResult(const UsageBackendResult& result)
{
    if (m_refreshCoordinator && m_refreshCoordinator->handleBackendResult(result)) {
        return;
    }

    if (result.kind == QLatin1String("providerConnectionTest")) {
        const QString requestProviderId = m_connectionTestRequestProviderIds.take(result.requestId);
        if (!result.success) {
            qWarning() << "Provider connection test backend job failed:" << requestProviderId << result.message;
            if (requestProviderId.isEmpty()) {
                return;
            }
            const qint64 finishedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();
            setProviderConnectionTest(requestProviderId, {
                {"state", "failed"},
                {"message", result.message.isEmpty() ? QStringLiteral("Connection failed") : result.message},
                {"details", result.message},
                {"startedAt", 0},
                {"finishedAt", finishedAt},
                {"durationMs", 0}
            });
            if (m_refreshCoordinator) {
                m_refreshCoordinator->applySnapshotUpdate(requestProviderId, ProviderFetchResult{});
            }
            return;
        }

        const auto payload = result.payload.value<ProviderConnectionTestPayload>();
        applyCredentialCacheUpdates(payload.credentialUpdates);
        applyProviderConnectionTestResult(payload.providerId, payload.fetchResult, payload.startedAt);
        return;
    }

    if (result.kind == QLatin1String("providerStatuses")) {
        if (m_statusManager) {
            m_statusManager->handleBackendResult(result.generation, result.success, result.payload);
        }
        return;
    }

    if (result.kind == QLatin1String("providerListModel")) {
        if (!result.success) {
            qWarning() << "Provider list backend job failed:" << result.message;
            return;
        }

        const auto payload = result.payload.value<ProviderListPayload>();
        if (m_uiService->handleProviderListResult(result.generation, payload.providers)) {
            m_costUsageDetailsRowsCacheValid = false;
            m_costUsageProviderDetailCache.clear();
            m_costUsageProviderDetailQueued.clear();
            m_costUsageDetailsRowsBuildQueued = false;
            ++m_costUsageDetailsRowsBuildGeneration;
            ++m_costUsageProviderDetailBuildGeneration;
        }
        return;
    }

    if (result.kind == QLatin1String("providerDescriptorData")) {
        const auto payload = result.payload.value<ProviderDescriptorDataPayload>();
        const QString providerId = payload.providerId;
        if (!result.success) {
            qWarning() << "Provider descriptor backend job failed:" << providerId << result.message;
            emit providerDescriptorChanged(providerId);
            return;
        }

        m_uiService->handleDescriptorResult(providerId, result.generation, payload.descriptor);
        emit providerDescriptorChanged(providerId);
        return;
    }

    if (result.kind == QLatin1String("codexCreditsRefresh")) {
        const CodexAccountRefreshGuard expectedGuard = m_backendCodexCreditGuards.take(result.requestId);
        CodexCreditsFetcher::FetchResult fetchResult;
        if (!result.success) {
            fetchResult.success = false;
            fetchResult.errorMessage = result.message;
        } else {
            fetchResult = result.payload.value<CodexCreditsRefreshPayload>().result;
        }
        m_codexCreditsRefreshing = false;
        applyCodexCreditsFetchResult(fetchResult, expectedGuard);
        return;
    }

    if (result.kind == QLatin1String("credentialStatusCheck")) {
        const auto payload = result.payload.value<CredentialStatusPayload>();
        {
            QMutexLocker locker(&m_credentialCacheMutex);
            m_credentialStatusInFlight.remove(payload.target);
            if (result.success && payload.exists) {
                m_credentialExisting.insert(payload.target);
                m_credentialMissing.remove(payload.target);
            } else {
                m_credentialExisting.remove(payload.target);
                m_credentialMissing[payload.target] = true;
            }
        }
        emit providerSecretChanged(payload.providerId, payload.key);
        return;
    }

    if (result.kind == QLatin1String("credentialPreload")) {
        if (!result.success) {
            qWarning() << "Credential preload backend job failed:" << result.message;
            return;
        }
        const auto payload = result.payload.value<CredentialPreloadPayload>();
        applyCredentialCacheUpdates(payload.updates);
        return;
    }

    if (result.kind == QLatin1String("providerSecretWrite")
        || result.kind == QLatin1String("providerSecretRemove")) {
        const auto payload = result.payload.value<ProviderSecretResultPayload>();
        const QByteArray secret = m_backendSecretValues.take(result.requestId);
        if (result.success && payload.success) {
            QMutexLocker locker(&m_credentialCacheMutex);
            if (payload.removed) {
                m_credentialCache.remove(payload.target);
                m_credentialExisting.remove(payload.target);
                m_credentialMissing[payload.target] = true;
            } else {
                m_credentialCache[payload.target] = {secret, QDateTime::currentDateTime()};
                m_credentialExisting.insert(payload.target);
                m_credentialMissing.remove(payload.target);
            }
            m_credentialStatusInFlight.remove(payload.target);
        }
        emit providerSecretChanged(payload.providerId, payload.key);
        emit providerConnectionTestChanged(payload.providerId);
        return;
    }

    if (result.kind == QLatin1String("providerLoginStart")) {
        const auto payload = result.payload.value<ProviderLoginStartPayload>();
        if (!result.success || !payload.success) {
            setProviderLoginState(payload.providerId, {
                {"state", "failed"},
                {"message", payload.message.isEmpty()
                    ? QStringLiteral("Could not start GitHub device login")
                    : payload.message}
            });
            m_loginManager->removeCancelFlag(payload.providerId);
            return;
        }

        setProviderLoginState(payload.providerId, {
            {"state", "verification"},
            {"message", "Enter the code in GitHub to authorize Copilot."},
            {"userCode", payload.userCode},
            {"verificationUri", payload.verificationUri},
            {"expiresIn", payload.expiresIn}
        });

        dispatchProviderLoginPoll(payload, m_loginManager->cancelFlag(payload.providerId));
        return;
    }

    if (result.kind == QLatin1String("providerLoginPoll")) {
        const auto payload = result.payload.value<ProviderLoginPollPayload>();
        setProviderLoginState(payload.providerId, {
            {"state", payload.state},
            {"message", payload.message}
        });
        m_loginManager->removeCancelFlag(payload.providerId);
        if (payload.triggerConnectionTest) {
            testProviderConnection(payload.providerId);
        }
        return;
    }

    if (result.kind == QLatin1String("costUsageSummary")) {
        if (result.generation != m_costUsageSummaryBuildGeneration) {
            return;
        }
        m_costUsageSummaryBuildQueued = false;
        if (!result.success) {
            qWarning() << "Cost usage summary backend job failed:" << result.message;
            return;
        }

        const auto payload = result.payload.value<CostUsageSummaryPayload>();
        m_costUsageDataCache = payload.costData;
        m_costUsageDataCacheValid = true;
        emit costUsageChanged();
        return;
    }

    if (result.kind == QLatin1String("costUsageProviderRows")) {
        if (result.generation != m_costUsageProviderRowsBuildGeneration) {
            return;
        }
        m_costUsageProviderRowsBuildQueued = false;
        if (!result.success) {
            qWarning() << "Cost usage provider rows backend job failed:" << result.message;
            return;
        }

        const auto payload = result.payload.value<CostUsageProviderRowsPayload>();
        m_providerCostUsageListCache = payload.providerList;
        m_providerCostUsageListCacheValid = true;
        emit costUsageChanged();
        return;
    }

    if (result.kind == QLatin1String("costUsageDetailsRows")) {
        if (result.generation != m_costUsageDetailsRowsBuildGeneration) {
            return;
        }
        m_costUsageDetailsRowsBuildQueued = false;
        if (!result.success) {
            qWarning() << "Cost usage details rows backend job failed:" << result.message;
            return;
        }

        const auto payload = result.payload.value<CostUsageDetailsRowsPayload>();
        m_costUsageDetailsRowsCache = payload.detailsRows;
        m_costUsageTokenProviderCountCache = payload.tokenProviderCount;
        m_costUsageDetailsRowsCacheValid = true;
        emit costUsageChanged();
        return;
    }

    if (result.kind == QLatin1String("costUsageProviderDetail")) {
        const auto payload = result.payload.value<CostUsageProviderDetailPayload>();
        const QString providerId = payload.providerId;
        m_costUsageProviderDetailQueued.remove(providerId);
        if (result.generation != m_costUsageProviderDetailBuildGeneration) {
            return;
        }
        if (!result.success) {
            qWarning() << "Cost usage provider detail backend job failed:" << result.message;
            return;
        }
        m_costUsageProviderDetailCache.insert(providerId, payload.detail);
        emit costUsageProviderDetailChanged(providerId);
        return;
    }

    if (result.kind == QLatin1String("costUsageRefresh")) {
        if (result.generation != m_costUsageRefreshGeneration) {
            return;
        }
        if (!result.success) {
            qWarning() << "Cost usage refresh backend job failed:" << result.message;
            m_costUsageRefreshing = false;
            emit costUsageRefreshingChanged();
            return;
        }

        PERF_PROBE("refreshCostUsage_callback", 2000);
        const auto payload = result.payload.value<CostUsageRefreshPayload>();
        m_costUsage = payload.combined;
        m_perProviderCostUsage = payload.perProvider;
        m_allProviderCostUsage = payload.allProviders;
        m_costUsageRefreshing = false;
        resetCostUsageDerivedCaches(false);
        emit costUsageRefreshingChanged();
        emit costUsageChanged();
        return;
    }

    if (result.kind == QLatin1String("codexAccountReconciliation")) {
        if (!result.success) {
            qWarning() << "Codex account reconciliation backend job failed:" << result.message;
            return;
        }
        const auto payload = result.payload.value<CodexReconciliationPayload>();
        if (m_codexAccountService) {
            m_codexAccountService->applySnapshot(payload.snapshot);
        }
        return;
    }

    if (result.kind.startsWith(QLatin1String("tokenAccount."))) {
        m_tokenAccountManager->handleBackendResult(
            result.kind,
            result.payload.toMap(),
            result.success,
            result.message);
        return;
    }
}

QHash<QString, QString> UsageStore::codexCreditsEnvironment() const
{
    QHash<QString, QString> env = cachedSystemEnv();
    if (m_codexAccountService) {
        QString managedHome = m_codexAccountService->activeManagedHomePath();
        if (!managedHome.isEmpty()) {
            env.insert(QStringLiteral("CODEX_HOME"), managedHome);
        }
    }
    return env;
}

void UsageStore::dispatchCodexCreditsRefresh(const QHash<QString, QString>& env,
                                             const CodexAccountRefreshGuard& expectedGuard)
{
    ++m_pendingCreditsRefresh;
    if (m_refreshCoordinator) {
        m_refreshCoordinator->incrementPendingExternalWork();
    }
    m_codexCreditsRefreshing = true;
    const UsageBackendRequest request = m_backend->dispatchValueJob(
        QStringLiteral("codexCreditsRefresh"), 0, [env]() -> QVariant {
        CodexCreditsFetcher fetcher(env);
        CodexCreditsRefreshPayload payload;
        payload.result = fetcher.fetchCreditsSync(ProviderPipeline::STRATEGY_TIMEOUT_MS);
        return QVariant::fromValue(payload);
    });
    m_backendCodexCreditGuards.insert(request.requestId, expectedGuard);
}

void UsageStore::dispatchProviderLoginPoll(const ProviderLoginStartPayload& startPayload,
                                           const QSharedPointer<QAtomicInt>& cancelFlag)
{
    if (!cancelFlag) {
        ProviderLoginPollPayload payload;
        payload.providerId = startPayload.providerId;
        payload.state = QStringLiteral("cancelled");
        payload.message = QStringLiteral("Login cancelled");
        setProviderLoginState(payload.providerId, {
            {"state", payload.state},
            {"message", payload.message}
        });
        return;
    }

    m_backend->dispatchValueJob(QStringLiteral("providerLoginPoll"), 0,
                                [startPayload, cancelFlag]() -> QVariant {
        ProviderLoginPollPayload payload;
        payload.providerId = startPayload.providerId;

        int interval = qMax(1, startPayload.interval);
        const QDateTime deadline = QDateTime::currentDateTimeUtc().addSecs(startPayload.expiresIn);
        while (QDateTime::currentDateTimeUtc() < deadline) {
            if (cancelFlag->loadAcquire() != 0) {
                payload.state = QStringLiteral("cancelled");
                payload.message = QStringLiteral("Login cancelled");
                return QVariant::fromValue(payload);
            }

            QThread::sleep(static_cast<unsigned long>(qMax(1, interval)));

            QUrlQuery tokenBody;
            tokenBody.addQueryItem(QStringLiteral("client_id"), QStringLiteral("Iv1.b507a08c87ecfe98"));
            tokenBody.addQueryItem(QStringLiteral("device_code"), startPayload.deviceCode);
            tokenBody.addQueryItem(QStringLiteral("grant_type"),
                                   QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));

            QJsonObject tokenResp = NetworkManager::instance().postFormSync(
                QUrl(QStringLiteral("https://github.com/login/oauth/access_token")),
                tokenBody.toString(QUrl::FullyEncoded).toUtf8(),
                {{QStringLiteral("Accept"), QStringLiteral("application/json")}});

            const QString errorType = tokenResp.value(QStringLiteral("error")).toString();
            if (errorType == QStringLiteral("authorization_pending")) continue;
            if (errorType == QStringLiteral("slow_down")) {
                interval += 5;
                continue;
            }
            if (!errorType.isEmpty()) {
                payload.state = QStringLiteral("failed");
                payload.message = errorType;
                return QVariant::fromValue(payload);
            }

            const QString accessToken = tokenResp.value(QStringLiteral("access_token")).toString();
            if (!accessToken.isEmpty()) {
                const bool ok = ProviderCredentialStore::write(
                    QStringLiteral("com.codexbarx.oauth.copilot"),
                    {},
                    accessToken.toUtf8());
                payload.state = ok ? QStringLiteral("succeeded") : QStringLiteral("failed");
                payload.message = ok
                    ? QStringLiteral("Copilot login complete")
                    : QStringLiteral("Could not save Copilot OAuth token");
                payload.triggerConnectionTest = ok;
                return QVariant::fromValue(payload);
            }
        }

        payload.state = QStringLiteral("failed");
        payload.message = QStringLiteral("GitHub device login expired");
        return QVariant::fromValue(payload);
    });
}

void UsageStore::prepareCodexRefreshForProviders(const QStringList& ids)
{
    auto currentGuard = currentCodexAccountRefreshGuard();
    if (currentGuard != m_lastCodexRefreshGuard) {
        clearCodexOpenAIWebState();
        m_lastCodexRefreshGuard = currentGuard;
    }

    if (ids.contains(QStringLiteral("codex")) && isProviderEnabled(QStringLiteral("codex"))) {
        auto expectedGuard = currentCodexAccountRefreshGuard();
        m_lastCodexRefreshGuard = expectedGuard;
        if (!expectedGuard.identity.isEmpty()) {
            dispatchCodexCreditsRefresh(codexCreditsEnvironment(), expectedGuard);
        }
    }
}

void UsageStore::refresh() {
    if (!m_refreshCoordinator) return;
    QStringList ids;
    for (const QString& id : m_providerCatalog.enabledProviderIDs()) ids.append(id);

    prepareCodexRefreshForProviders(ids);
    m_refreshCoordinator->refresh(ids);
}

void UsageStore::refreshAll() {
    if (!m_refreshCoordinator) return;
    QStringList ids;
    for (const QString& id : m_providerCatalog.providerIDs()) ids.append(id);

    prepareCodexRefreshForProviders(ids);
    m_refreshCoordinator->refresh(ids);
}

void UsageStore::clearCache() {
    const QVector<QString> ids = m_providerCatalog.providerIDs();
    if (m_refreshCoordinator) {
        m_refreshCoordinator->clearCache();
    }
    m_connectionTester->clearAll();
    {
        QMutexLocker locker(&m_credentialCacheMutex);
        m_credentialCache.clear();
        m_credentialMissing.clear();
    }
    m_uiService->invalidateSnapshotDataCache(QString());
    // Batch emit connection test changes to avoid signal storm
    for (const auto& id : ids) {
        emit providerConnectionTestChanged(id);
    }
}

void UsageStore::refreshProvider(const QString& providerId) {
    if (m_refreshCoordinator) {
        m_refreshCoordinator->refreshProvider(providerId);
    }
}

void UsageStore::onProviderRefreshSuccess(const QString& providerId,
                                           const ProviderFetchResult& result)
{
    if (m_historyStore) {
        m_historyStore->recordSample(providerId, result.usage);
    }

    auto sessionWindow = result.usage.primary;
    if (!sessionWindow.has_value() && result.usage.secondary.has_value()) {
        sessionWindow = result.usage.secondary;
    }
    if (sessionWindow.has_value()) {
        double currentRemaining = sessionWindow->remainingPercent();
        auto prevRemaining = m_lastKnownSessionRemaining.value(providerId);
        auto t = SessionQuotaNotificationLogic::transition(prevRemaining, currentRemaining);
        bool notificationsEnabled = m_settingsStore
            ? m_settingsStore->sessionQuotaNotificationsEnabled()
            : true;
        if (t != SessionQuotaTransition::None && notificationsEnabled) {
            QString name = providerDisplayName(providerId);
            SessionQuotaNotifier::post(t, name);
        }
        m_lastKnownSessionRemaining[providerId] = currentRemaining;
        if (providerId == "codex" && !result.sourceLabel.isEmpty() &&
            m_lastKnownSessionWindowSource != result.sourceLabel) {
            QString label = result.sourceLabel;
            bool hasAttachedDashboard = result.dashboard.has_value()
                && result.dashboard->toVariantMap().value("visibility", "hidden").toString() == "attached";
            if (hasAttachedDashboard) {
                label += " + openai-web";
            }
            m_lastKnownSessionWindowSource = label;
            emit lastKnownSessionWindowSourceChanged();
        }
    }

    // Codex-specific: refresh credits after successful usage fetch.
    if (providerId == "codex") {
        auto guard = currentCodexAccountRefreshGuard();
        // If doRefresh already scheduled an async credits fetch, avoid duplicate work.
        if (m_pendingCreditsRefresh == 0) {
            QString accountKey = currentCodexAccountKey();
            bool cacheFresh = !accountKey.isEmpty() &&
                m_codexCreditsCache.accountKey == accountKey &&
                m_codexCreditsCache.updatedAt.isValid() &&
                m_codexCreditsCache.updatedAt.secsTo(QDateTime::currentDateTime()) < 300;
            if (!cacheFresh) {
                dispatchCodexCreditsRefresh(codexCreditsEnvironment(), guard);
            }
        }
    }
}

void UsageStore::onProviderRefreshFailed(const QString& providerId,
                                          const QString& /*errorMessage*/)
{
    Q_UNUSED(providerId)
}

void UsageStore::startAutoRefresh(int intervalMinutes) {
    if (m_refreshCoordinator) {
        m_refreshCoordinator->startAutoRefresh(intervalMinutes);
    }
}

void UsageStore::stopAutoRefresh() {
    if (m_refreshCoordinator) {
        m_refreshCoordinator->stopAutoRefresh();
    }
}

QString UsageStore::error(const QString& providerId) const {
    if (!m_refreshCoordinator) return {};
    return Localization::providerError(m_refreshCoordinator->error(providerId));
}

QString UsageStore::providerError(const QString& providerId) const {
    if (!m_refreshCoordinator) return {};
    return Localization::providerError(m_refreshCoordinator->error(providerId));
}

QStringList UsageStore::allProviderIDs() const {
    QStringList ids;
    for (const QString& id : m_providerCatalog.providerIDs()) ids.append(id);
    return ids;
}

QVariantList UsageStore::utilizationChartData(const QString& providerId, const QString& seriesName) const {
    if (!m_historyStore) return {};
    return m_historyStore->chartData(providerId, seriesName);
}

QVariantList UsageStore::providerList() const {
    return m_uiService->providerList();
}

void UsageStore::requestProviderList()
{
    m_uiService->requestProviderList();
}

void UsageStore::moveProvider(int fromIndex, int toIndex) {
    if (!m_settingsStore) return;
    if (fromIndex == toIndex) return;

    auto list = providerList();
    if (fromIndex < 0 || fromIndex >= list.size()) return;
    if (toIndex < 0 || toIndex >= list.size()) return;

    QStringList order = m_settingsStore->providerOrder();
    if (order.isEmpty()) {
        // Initialize order from current list
        for (const auto& item : list) {
            order.append(item.toMap().value("id").toString());
        }
    }

    if (fromIndex < order.size() && toIndex < order.size()) {
        order.move(fromIndex, toIndex);
        m_settingsStore->setProviderOrder(order);
        rebuildProviderCatalogSnapshot();
        m_uiService->invalidateProviderListCache();
        emit providerIDsChanged();
    }
}

QVariantMap UsageStore::providerDescriptorData(const QString& id) const {
    return m_uiService->providerDescriptorData(id);
}

void UsageStore::requestProviderDescriptor(const QString& providerId)
{
    m_uiService->requestProviderDescriptor(providerId);
}

void UsageStore::setProviderSetting(const QString& providerId, const QString& key, const QVariant& value) {
    auto descriptor = settingDescriptor(providerId, key);
    if (descriptor.has_value() && descriptor->sensitive) {
        return;
    }
    if (!m_settingsStore) {
        return;
    }
    m_settingsStore->setProviderSetting(providerId, key, value);
    m_uiService->invalidateDescriptorCache(providerId);
    if (key == QLatin1String("sourceMode")) {
        rebuildProviderCatalogSnapshot();
        m_uiService->invalidateProviderListCache();
        emit providerIDsChanged();
    }
    emit providerDescriptorChanged(providerId);
}

std::optional<ProviderSettingsDescriptor> UsageStore::settingDescriptor(const QString& providerId,
                                                                        const QString& key) const
{
    const auto entry = m_providerCatalog.provider(providerId);
    if (!entry.has_value()) return std::nullopt;
    for (const auto& descriptor : entry->settingsDescriptors) {
        if (descriptor.key == key) return descriptor;
    }
    return std::nullopt;
}

QVariantMap UsageStore::providerSecretStatus(const QString& providerId, const QString& key) const {
    PERF_PROBE("providerSecretStatus", 500);
    QVariantMap status;
    status["configured"] = false;
    status["source"] = "none";

    auto descriptor = settingDescriptor(providerId, key);
    if (!descriptor.has_value() || !descriptor->sensitive) return status;

    status["envVar"] = descriptor->envVar;
    status["credentialTarget"] = descriptor->credentialTarget;

    // Check env var — use cached snapshot to avoid QProcessEnvironment::systemEnvironment()
    // on the main thread. Rebuild once on first access; tests call rebuildSystemEnvCache()
    // after qputenv() to pick up new variables.
    if (!descriptor->envVar.isEmpty()) {
        const auto& cachedEnv = cachedSystemEnv();
        if (cachedEnv.contains(descriptor->envVar)) {
            status["configured"] = true;
            status["source"] = "env";
            status["readOnly"] = true;
            return status;
        }
    }

    if (!descriptor->credentialTarget.isEmpty()) {
        bool credentialExists = false;
        bool credentialCheckPending = false;
        {
            QMutexLocker locker(&m_credentialCacheMutex);
            if (m_credentialCache.contains(descriptor->credentialTarget)
                || m_credentialExisting.contains(descriptor->credentialTarget)) {
                credentialExists = true;
            } else if (!m_credentialMissing.contains(descriptor->credentialTarget)) {
                credentialCheckPending = true;
            }
        }

        if (credentialExists) {
            status["configured"] = true;
            status["source"] = "credential";
            return status;
        }
        if (credentialCheckPending) {
            status["checking"] = true;
            status["source"] = "checking";
            queueCredentialStatusCheck(providerId, key, descriptor->credentialTarget);
            return status;
        }
    }

    // Known-missing credentials fall through without probing WinCred here.
    if (!descriptor->credentialTarget.isEmpty()) {
        return status;
    }

    if (m_settingsStore
        && m_settingsStore->providerSetting(providerId, key).isValid()
        && !m_settingsStore->providerSetting(providerId, key).toString().isEmpty()) {
        status["configured"] = true;
        status["source"] = "settings";
    }
    return status;
}

void UsageStore::queueCredentialStatusCheck(const QString& providerId,
                                            const QString& key,
                                            const QString& target) const
{
    if (target.isEmpty()) return;

    {
        QMutexLocker locker(&m_credentialCacheMutex);
        if (m_credentialCache.contains(target)
            || m_credentialExisting.contains(target)
            || m_credentialMissing.contains(target)
            || m_credentialStatusInFlight.contains(target)) {
            return;
        }
        m_credentialStatusInFlight.insert(target);
    }

    auto* self = const_cast<UsageStore*>(this);
    self->m_backend->dispatchValueJob(QStringLiteral("credentialStatusCheck"), 0,
                                      [providerId, key, target]() -> QVariant {
        CredentialStatusPayload payload;
        payload.providerId = providerId;
        payload.key = key;
        payload.target = target;
        payload.exists = ProviderCredentialStore::exists(target);
        return QVariant::fromValue(payload);
    });
}

bool UsageStore::setProviderSecret(const QString& providerId,
                                   const QString& key,
                                   const QString& value)
{
    auto descriptor = settingDescriptor(providerId, key);
    if (!descriptor.has_value() || !descriptor->sensitive) {
        return false;
    }

    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) return false;

    if (!descriptor->credentialTarget.isEmpty()) {
        QString target = descriptor->credentialTarget;
        const QByteArray secret = trimmed.toUtf8();
        const UsageBackendRequest request = m_backend->dispatchValueJob(
            QStringLiteral("providerSecretWrite"), 0,
            [target, providerId, key, secret]() -> QVariant {
            ProviderSecretResultPayload payload;
            payload.providerId = providerId;
            payload.key = key;
            payload.target = target;
            payload.success = ProviderCredentialStore::write(target, {}, secret);
            payload.removed = false;
            return QVariant::fromValue(payload);
        });
        m_backendSecretValues.insert(request.requestId, secret);
        return true; // Optimistic success
    } else {
        // Fall back to settings store when no credential target is configured
        if (m_settingsStore) {
            m_settingsStore->setProviderSetting(providerId, key, trimmed);
        }
        emit providerSecretChanged(providerId, key);
        emit providerConnectionTestChanged(providerId);
        return true;
    }
}

bool UsageStore::clearProviderSecret(const QString& providerId, const QString& key) {
    auto descriptor = settingDescriptor(providerId, key);
    if (!descriptor.has_value() || !descriptor->sensitive) {
        return false;
    }

    if (!descriptor->credentialTarget.isEmpty()) {
        QString target = descriptor->credentialTarget;
        m_backend->dispatchValueJob(QStringLiteral("providerSecretRemove"), 0,
                                    [target, providerId, key]() -> QVariant {
            ProviderCredentialStore::remove(target);
            ProviderSecretResultPayload payload;
            payload.providerId = providerId;
            payload.key = key;
            payload.target = target;
            payload.success = true;
            payload.removed = true;
            return QVariant::fromValue(payload);
        });
        return true; // Optimistic success
    } else {
        // Fall back to settings store when no credential target is configured
        if (m_settingsStore) {
            m_settingsStore->setProviderSetting(providerId, key, QVariant());
        }
        emit providerSecretChanged(providerId, key);
        emit providerConnectionTestChanged(providerId);
        return true;
    }
}

void UsageStore::setProviderConnectionTest(const QString& providerId, const QVariantMap& state) {
    m_connectionTester->setTestState(providerId, state);
}

QVariantMap UsageStore::providerConnectionTest(const QString& providerId) const {
    return m_connectionTester->testState(providerId);
}

void UsageStore::testProviderConnection(const QString& providerId) {
    const qint64 startedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();
    auto failNow = [&](const QString& message, const QString& details = QString()) {
        const qint64 finishedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();
        setProviderConnectionTest(providerId, {
            {"state", "failed"},
            {"message", message},
            {"details", details.isEmpty() ? message : details},
            {"startedAt", startedAt},
            {"finishedAt", finishedAt},
            {"durationMs", finishedAt - startedAt}
        });
    };

    auto* provider = ProviderRegistry::instance().provider(providerId);
    if (!provider) {
        failNow("Unknown provider");
        return;
    }

    qDebug() << "[TestConnection] Starting test for provider:" << providerId;
    setProviderConnectionTest(providerId, {
        {"state", "testing"},
        {"message", "Testing connection..."},
        {"details", ""},
        {"startedAt", startedAt},
        {"finishedAt", 0},
        {"durationMs", 0}
    });

    const UsageBackendJobs::ProviderFetchCommandInput input = buildProviderFetchCommandInput(providerId);
    const UsageBackendRequest request = m_backend->dispatchValueJob(
        QStringLiteral("providerConnectionTest"), 0,
        [provider, input, startedAt]() {
        return QVariant::fromValue(UsageBackendJobs::testProviderConnection(provider, input, startedAt));
    });
    m_connectionTestRequestProviderIds.insert(request.requestId, providerId);
}

void UsageStore::applyProviderConnectionTestResult(const QString& providerId,
                                                   const ProviderFetchResult& result,
                                                   qint64 startedAt)
{
    PERF_PROBE("testProviderConnection_callback", 2000);
    const qint64 finishedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();
    qDebug() << "[TestConnection] Provider:" << providerId << "success:" << result.success << "error:" << result.errorMessage;
    if (m_refreshCoordinator) {
        m_refreshCoordinator->applySnapshotUpdate(providerId, result);
    }
    if (result.success) {
        setProviderConnectionTest(providerId, {
            {"state", "succeeded"},
            {"message", "Connection OK"},
            {"details", ""},
            {"startedAt", startedAt},
            {"finishedAt", finishedAt},
            {"durationMs", finishedAt - startedAt}
        });
    } else {
        QString message = result.errorMessage.trimmed();
        if (message.isEmpty()) message = "Connection failed";
        setProviderConnectionTest(providerId, {
            {"state", "failed"},
            {"message", message},
            {"details", result.errorMessage},
            {"startedAt", startedAt},
            {"finishedAt", finishedAt},
            {"durationMs", finishedAt - startedAt}
        });
    }
}

void UsageStore::setProviderLoginState(const QString& providerId, const QVariantMap& state) {
    m_loginManager->setLoginState(providerId, state);
}

QVariantMap UsageStore::providerLoginState(const QString& providerId) const {
    return m_loginManager->loginState(providerId);
}

void UsageStore::startProviderLogin(const QString& providerId) {
    if (providerId != "copilot") {
        setProviderLoginState(providerId, {{"state", "failed"}, {"message", "Login is not available for this provider"}});
        return;
    }

    auto cancelFlag = QSharedPointer<QAtomicInt>::create(0);
    m_loginManager->setCancelFlag(providerId, cancelFlag);
    setProviderLoginState(providerId, {{"state", "starting"}, {"message", "Requesting device code..."}});

    m_backend->dispatchValueJob(QStringLiteral("providerLoginStart"), 0, [providerId]() -> QVariant {
        ProviderLoginStartPayload payload;
        payload.providerId = providerId;

        QUrlQuery deviceBody;
        deviceBody.addQueryItem(QStringLiteral("client_id"), QStringLiteral("Iv1.b507a08c87ecfe98"));
        deviceBody.addQueryItem(QStringLiteral("scope"), QStringLiteral("read:user"));

        QJsonObject deviceResp = NetworkManager::instance().postFormSync(
            QUrl(QStringLiteral("https://github.com/login/device/code")),
            deviceBody.toString(QUrl::FullyEncoded).toUtf8(),
            {{QStringLiteral("Accept"), QStringLiteral("application/json")}});

        payload.deviceCode = deviceResp.value(QStringLiteral("device_code")).toString();
        payload.userCode = deviceResp.value(QStringLiteral("user_code")).toString();
        payload.verificationUri = deviceResp.value(QStringLiteral("verification_uri")).toString(
            QStringLiteral("https://github.com/login/device"));
        payload.interval = deviceResp.value(QStringLiteral("interval")).toInt(5);
        payload.expiresIn = deviceResp.value(QStringLiteral("expires_in")).toInt(900);

        if (payload.deviceCode.isEmpty() || payload.userCode.isEmpty()) {
            payload.success = false;
            payload.message = QStringLiteral("Could not start GitHub device login");
            return QVariant::fromValue(payload);
        }

        payload.success = true;
        return QVariant::fromValue(payload);
    });
}

void UsageStore::cancelProviderLogin(const QString& providerId) {
    auto flag = m_loginManager->cancelFlag(providerId);
    if (flag) flag->storeRelease(1);
}

void UsageStore::setProviderStatus(const QString& providerId, const QVariantMap& status) {
    m_statusManager->setStatus(providerId, status);
    m_uiService->invalidateProviderListCache();
}

QVariantMap UsageStore::providerStatus(const QString& providerId) const {
    return m_statusManager->status(providerId);
}

QString UsageStore::providerStatusURL(const QString& providerId) const {
    const auto entry = m_providerCatalog.provider(providerId);
    if (!entry.has_value() || !entry->hasDescriptor) return {};
    const auto& desc = entry->descriptor;
    return ProviderStatusFetcher::openURL(
        desc.metadata.statusPageURL,
        desc.metadata.statusLinkURL,
        desc.metadata.statusWorkspaceProductID);
}

QVariantMap UsageStore::providerUsageSnapshot(const QString& providerId) const {
    UsageSnapshot snap = snapshot(providerId);
    if (!snap.updatedAt.isValid()) return {};
    return m_uiService ? m_uiService->providerUsageSnapshot(providerId, snap) : QVariantMap{};
}

void UsageStore::refreshProviderStatuses() {
    if (m_statusManager) {
        m_statusManager->refreshStatuses(m_providerCatalog, m_backend);
    }
}

// ============================================================================
// Codex Multi-Account Management
// ============================================================================

QVariantList UsageStore::codexAccounts() const
{
    QVariantList result;
    if (!m_codexAccountService) return result;

    auto accounts = m_codexAccountService->visibleAccounts();
    for (const auto& account : accounts) {
        QVariantMap map;
        map["id"] = account.id;
        map["displayName"] = account.displayName;
        map["email"] = account.email;
        map["workspaceLabel"] = account.workspaceLabel;
        map["isLive"] = account.isLive;
        map["isActive"] = account.isActive;
        map["storedAccountID"] = account.storedAccountID;
        map["canReauthenticate"] = account.canReauthenticate;
        map["canRemove"] = account.canRemove;
        map["selectionSource"] = CodexActiveSourceUtil::toString(account.selectionSource);
        result.append(map);
    }
    return result;
}

QVariantMap UsageStore::codexAccountState() const
{
    QVariantMap state;
    state["accounts"] = codexAccounts();
    state["activeAccountID"] = codexActiveAccountID();
    state["isAuthenticating"] = isCodexAuthenticating();
    state["isRemoving"] = isCodexRemoving();
    state["isPromoting"] = m_isPromoting;
    state["authenticatingAccountID"] = codexAuthenticatingAccountID();
    state["removingAccountID"] = codexRemovingAccountID();
    state["promotingAccountID"] = m_promotingAccountID;
    state["hasUnreadableStore"] = hasCodexUnreadableStore();

    QString authState = "idle";
    if (m_codexAccountService) {
        if (m_codexAccountService->isAuthenticating()) {
            authState = m_codexAccountService->authUserCode().isEmpty()
                ? QStringLiteral("starting")
                : QStringLiteral("verification");
        } else if (!m_codexAccountService->authError().isEmpty()) {
            authState = QStringLiteral("failed");
        } else if (!m_codexAccountService->authMessage().isEmpty()) {
            authState = QStringLiteral("succeeded");
        }

        state["authState"] = authState;
        state["authMessage"] = m_codexAccountService->authMessage();
        state["authError"] = m_codexAccountService->authError();
        state["verificationUri"] = m_codexAccountService->authVerificationUri();
        state["userCode"] = m_codexAccountService->authUserCode();
    } else {
        state["authState"] = authState;
        state["authMessage"] = QString();
        state["authError"] = QString();
        state["verificationUri"] = QString();
        state["userCode"] = QString();
    }

    return state;
}

QString UsageStore::codexActiveAccountID() const
{
    if (!m_codexAccountService) return "live-system";
    return m_codexAccountService->activeAccountID();
}

void UsageStore::setCodexActiveAccount(const QString& accountID)
{
    if (!m_codexAccountService) return;

    QString previousID = m_codexAccountService->activeAccountID();
    if (previousID == accountID) return;

    clearCodexOpenAIWebState();
    m_lastCodexRefreshGuard = {}; // force guard re-evaluation on next refresh

    m_codexAccountService->setActiveAccount(accountID);

    emit codexActiveAccountChanged(accountID);
    emit codexAccountsChanged();

    refreshProvider("codex");
}

bool UsageStore::addCodexAccount(const QString& email, const QString& homePath)
{
    if (!m_codexAccountService) return false;

    clearCodexOpenAIWebState();
    m_lastCodexRefreshGuard = {};

    if (email.isEmpty()) {
        return m_codexAccountService->authenticateNewAccount();
    }

    return m_codexAccountService->addAccount(email, homePath);
}

void UsageStore::cancelCodexAuthentication()
{
    if (!m_codexAccountService) return;
    m_codexAccountService->cancelAuthentication();
}

bool UsageStore::removeCodexAccount(const QString& accountID)
{
    if (!m_codexAccountService) return false;
    return m_codexAccountService->removeAccount(accountID);
}

bool UsageStore::reauthenticateCodexAccount(const QString& accountID)
{
    if (!m_codexAccountService) return false;
    return m_codexAccountService->reauthenticateAccount(accountID);
}

bool UsageStore::promoteCodexAccount(const QString& accountID)
{
    if (!m_codexAccountService) return false;
    if (m_isPromoting) return false;

    // Set promoting state
    m_isPromoting = true;
    m_promotingAccountID = accountID;
    emit codexAccountStateChanged();

    // Clear state before promotion
    clearCodexOpenAIWebState();
    m_lastCodexRefreshGuard = {};

    bool ok = m_codexAccountService->promoteAccount(accountID);

    // Clear promoting state
    m_isPromoting = false;
    m_promotingAccountID.clear();
    emit codexAccountStateChanged();

    if (ok) {
        refreshProvider("codex");
    }

    return ok;
}

bool UsageStore::isCodexAuthenticating() const
{
    if (!m_codexAccountService) return false;
    return m_codexAccountService->isAuthenticating();
}

bool UsageStore::isCodexRemoving() const
{
    if (!m_codexAccountService) return false;
    return m_codexAccountService->isRemoving();
}

QString UsageStore::codexAuthenticatingAccountID() const
{
    if (!m_codexAccountService) return QString();
    return m_codexAccountService->authenticatingAccountID();
}

QString UsageStore::codexRemovingAccountID() const
{
    if (!m_codexAccountService) return QString();
    return m_codexAccountService->removingAccountID();
}

bool UsageStore::hasCodexUnreadableStore() const
{
    if (!m_codexAccountService) return false;
    return m_codexAccountService->hasUnreadableStore();
}

// ============================================================================
// Codex Account Refresh Guard (mirrors original CodexBar)
// ============================================================================

UsageStore::CodexAccountRefreshGuard UsageStore::currentCodexAccountRefreshGuard() const
{
    CodexAccountRefreshGuard guard;
    guard.accountKey = currentCodexAccountKey();

    if (!m_codexAccountService) {
        guard.source = "liveSystem";
        auto snap = snapshot("codex");
        if (snap.identity.has_value() && snap.identity->accountEmail.has_value()) {
            guard.identity = snap.identity->accountEmail.value().toLower();
        }
        return guard;
    }

    QString activeId = m_codexAccountService->activeAccountID();
    if (activeId.isEmpty() || activeId == "live-system") {
        guard.source = "liveSystem";
        auto snap = snapshot("codex");
        if (snap.identity.has_value() && snap.identity->accountEmail.has_value()) {
            guard.identity = snap.identity->accountEmail.value().toLower();
        }
    } else {
        guard.source = "managedAccount";
        guard.identity = activeId.toLower();
    }
    return guard;
}

bool UsageStore::shouldApplyCodexScopedNonUsageResult(const CodexAccountRefreshGuard& expectedGuard) const
{
    auto currentGuard = currentCodexAccountRefreshGuard();
    if (currentGuard.source != expectedGuard.source) return false;
    if (expectedGuard.identity.isEmpty()) return false;
    return currentGuard.identity == expectedGuard.identity;
}

UsageSnapshot UsageStore::waitForCodexSnapshot(const QDateTime& minimumUpdatedAt, int timeoutMs) const
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (NetworkManager::isShuttingDown()) break;
        auto snap = snapshot("codex");
        if (snap.updatedAt.isValid() && snap.updatedAt >= minimumUpdatedAt) {
            return snap;
        }
        // Short sleep for faster response; max 3s default instead of 6s.
        QThread::msleep(50);
    }
    return snapshot("codex");
}

// ============================================================================
// Codex Credits Cache & Consumer Projection
// ============================================================================

QString UsageStore::currentCodexAccountKey() const
{
    auto snap = snapshot("codex");
    if (snap.identity.has_value() && snap.identity->accountEmail.has_value()) {
        QString email = snap.identity->accountEmail.value().toLower().trimmed();
        QByteArray hashBytes = QCryptographicHash::hash(email.toUtf8(), QCryptographicHash::Sha256);
        return "codex:v1:email-hash:" + QString::fromLatin1(hashBytes.toHex());
    }
    if (snap.identity.has_value() && snap.identity->loginMethod.has_value()) {
        QString method = snap.identity->loginMethod.value().toLower().trimmed();
        QByteArray hashBytes = QCryptographicHash::hash(method.toUtf8(), QCryptographicHash::Sha256);
        return "codex:v1:method-hash:" + QString::fromLatin1(hashBytes.toHex());
    }
    return "codex:v1:unresolved";
}

std::optional<CreditsSnapshot> UsageStore::cachedCodexCredits() const
{
    if (m_codexCreditsCache.accountKey == currentCodexAccountKey()) {
        return m_codexCreditsCache.snapshot;
    }
    return std::nullopt;
}

QString UsageStore::codexCreditsError() const
{
    if (m_codexCreditsCache.accountKey == currentCodexAccountKey()) {
        return m_codexCreditsCache.lastError;
    }
    return QString();
}

void UsageStore::refreshCodexCredits(const CodexAccountRefreshGuard& expectedGuard)
{
    if (!isProviderEnabled("codex")) return;

    QString accountKey = currentCodexAccountKey();
    if (accountKey.isEmpty()) return;

    // Check cache freshness (5 minute cooldown)
    if (m_codexCreditsCache.accountKey == accountKey &&
        m_codexCreditsCache.updatedAt.isValid() &&
        m_codexCreditsCache.updatedAt.secsTo(QDateTime::currentDateTime()) < 300) {
        return;
    }

    m_codexCreditsRefreshing = true;

    // Stage 4: test injection point
    if (_test_codexCreditsLoaderOverride) {
        auto overrideResult = _test_codexCreditsLoaderOverride();
        CodexCreditsFetcher::FetchResult result;
        if (overrideResult.has_value()) {
            result.success = true;
            result.credits = overrideResult;
        } else {
            result.success = false;
            result.errorMessage = "test override returned no credits";
        }
        applyCodexCreditsFetchResult(result, expectedGuard);
        m_codexCreditsRefreshing = false;
        return;
    }

    dispatchCodexCreditsRefresh(codexCreditsEnvironment(), expectedGuard);
}

void UsageStore::applyCodexCreditsFetchResult(const CodexCreditsFetcher::FetchResult& result,
                                               const CodexAccountRefreshGuard& expectedGuard)
{
    // Decrement pending counter
    if (m_pendingCreditsRefresh > 0) {
        --m_pendingCreditsRefresh;
    }
    if (m_refreshCoordinator) {
        m_refreshCoordinator->decrementPendingExternalWork();
    }

    // Guard check — discard result if account changed during fetch
    if (!expectedGuard.isEmpty() && !shouldApplyCodexScopedNonUsageResult(expectedGuard)) {
        qDebug() << "[UsageStore] Discarding stale credits result (account changed during fetch)";
        return;
    }

    QString accountKey = currentCodexAccountKey();
    if (accountKey.isEmpty()) {
        return;
    }

    QDateTime now = QDateTime::currentDateTime();

    if (result.success && result.credits.has_value()) {
        m_codexCreditsCache.snapshot = result.credits;
        m_codexCreditsCache.accountKey = accountKey;
        m_codexCreditsCache.updatedAt = now;
        m_codexCreditsCache.failureStreak = 0;
        m_codexCreditsCache.lastError.clear();
        emit codexCreditsChanged();

        // Plan history backfill if usage snapshot is stale
        if (m_historyStore) {
            auto snap = snapshot("codex");
            if (snap.updatedAt.isValid()) {
                m_historyStore->recordSample("codex", snap, accountKey);
            }
        }
    } else {
        QString errorMsg = result.errorMessage;
        if (errorMsg.isEmpty()) {
            errorMsg = "Codex credits are still loading; will retry shortly.";
        }

        m_codexCreditsCache.failureStreak++;
        m_codexCreditsCache.lastError = errorMsg;
        m_codexCreditsCache.accountKey = accountKey;
        m_codexCreditsCache.updatedAt = now;

        // Emit only if we have no stale cached value to show
        if (!m_codexCreditsCache.snapshot.has_value()) {
            emit codexCreditsChanged();
        }
    }
}

QVariantMap UsageStore::codexConsumerProjectionData() const
{
    PERF_PROBE("codexConsumerProjectionData", 2000);
    QVariantMap m;
    auto snap = snapshot("codex");

    CodexConsumerProjection::Context ctx;
    ctx.snapshot = snap;
    ctx.rawUsageError = m_refreshCoordinator ? m_refreshCoordinator->error("codex") : QString();
    ctx.now = QDateTime::currentDateTime();

    if (m_codexCreditsCache.snapshot.has_value() &&
        m_codexCreditsCache.accountKey == currentCodexAccountKey()) {
        ctx.credits = const_cast<CreditsSnapshot*>(&m_codexCreditsCache.snapshot.value());
        ctx.rawCreditsError = m_codexCreditsCache.lastError;
    }

    auto projection = CodexConsumerProjection::make(
        CodexConsumerProjection::Surface::LiveCard, ctx);

    m["dashboardVisibility"] = static_cast<int>(projection.dashboardVisibility);
    m["menuBarFallback"] = static_cast<int>(projection.menuBarFallback);
    m["hasExhaustedRateLane"] = CodexConsumerProjection::hasExhaustedRateLane(projection);

    // Credits
    if (projection.credits.has_value() && projection.credits->snapshot.has_value()) {
        m["hasCredits"] = true;
        m["creditsRemaining"] = projection.credits->snapshot->remaining;
        m["creditsError"] = projection.credits->userFacingError;
    } else {
        m["hasCredits"] = false;
    }

    // Consumer Projection: supplemental metrics
    QVariantList supplementalMetrics;
    for (const auto& metric : projection.supplementalMetrics) {
        supplementalMetrics.append(static_cast<int>(metric));
    }
    m["supplementalMetrics"] = supplementalMetrics;

    // Consumer Projection: buy credits
    m["canShowBuyCredits"] = projection.canShowBuyCredits;

    // Consumer Projection: usage breakdown
    m["hasUsageBreakdown"] = projection.hasUsageBreakdown;

    // Consumer Projection: credits history
    m["hasCreditsHistory"] = projection.hasCreditsHistory;

    // Consumer Projection: plan utilization lanes
    QVariantList planLanes;
    for (const auto& lane : projection.planUtilizationLanes) {
        QVariantMap laneMap;
        laneMap["role"] = static_cast<int>(lane.role);
        laneMap["usedPercent"] = lane.window.usedPercent;
        laneMap["remainingPercent"] = lane.window.remainingPercent();
        if (lane.window.resetsAt.has_value())
            laneMap["resetsAt"] = lane.window.resetsAt->toMSecsSinceEpoch();
        if (lane.window.windowMinutes.has_value())
            laneMap["windowMinutes"] = *lane.window.windowMinutes;
        planLanes.append(laneMap);
    }
    m["planUtilizationLanes"] = planLanes;

    // Consumer Projection: user-facing errors
    QVariantMap userErrors;
    userErrors["usage"] = projection.userFacingErrors.usage;
    userErrors["credits"] = projection.userFacingErrors.credits;
    userErrors["dashboard"] = projection.userFacingErrors.dashboard;
    m["userFacingErrors"] = userErrors;

    return m;
}

QVariantList UsageStore::codexFetchAttempts() const
{
    QVariantList list;
    if (!m_refreshCoordinator) return list;
    for (const auto& attempt : m_refreshCoordinator->fetchAttempts(QStringLiteral("codex"))) {
        list.append(attempt.toMap());
    }
    return list;
}

QString UsageStore::lastKnownSessionWindowSource() const
{
    return m_lastKnownSessionWindowSource;
}

void UsageStore::clearCodexOpenAIWebState()
{
    // Clear dashboard HTML cache
    CodexDashboardCache::clear();

    // Clear Codex-specific cached state
    m_codexCreditsCache = {};
    if (m_refreshCoordinator) {
        m_refreshCoordinator->removeSnapshot("codex");
    }
    m_connectionTester->clearTestState("codex");
    m_lastKnownSessionRemaining.remove("codex");
    m_lastKnownSessionWindowSource.clear();

    emit snapshotChanged("codex");
    m_uiService->invalidateSnapshotDataCache(QStringLiteral("codex"));
    emit snapshotRevisionChanged();
    emit codexCreditsChanged();
    emit codexFetchAttemptsChanged();
}

void UsageStore::shutdown()
{
    stopAutoRefresh();
    if (m_historyStore) {
        m_historyStore->stopSaveTimer();
    }
    NetworkManager::setShuttingDown(true);
    CostUsageService::setShuttingDown(true);
}

QVariantMap UsageStore::providerDashboardData(const QString& providerId) const
{
    return m_refreshCoordinator ? m_refreshCoordinator->dashboardData(providerId) : QVariantMap();
}

// Token Account management implementations

QVariantList UsageStore::tokenAccountsForProvider(const QString& providerId) const
{
    QVariantList result;
    auto accounts = TokenAccountStore::instance()->accountsForProviderMetadata(providerId);
    for (const auto& acc : accounts) {
        result.append(acc.toVariantMap());
    }
    return result;
}

QVariantMap UsageStore::tokenAccountOperationState() const
{
    return m_tokenAccountManager->operationState();
}

QString UsageStore::addTokenAccount(const QString& providerId, const QString& displayName, int sourceMode)
{
    return m_tokenAccountManager->addAccount(providerId, displayName, sourceMode);
}

QString UsageStore::addTokenAccountWithApiKey(const QString& providerId,
                                              const QString& displayName,
                                              int sourceMode,
                                              const QString& apiKey)
{
    return m_tokenAccountManager->addAccountWithApiKey(providerId, displayName, sourceMode, apiKey);
}

bool UsageStore::removeTokenAccount(const QString& accountId)
{
    return m_tokenAccountManager->removeAccount(accountId);
}

bool UsageStore::setTokenAccountVisibility(const QString& accountId, int visibility)
{
    return m_tokenAccountManager->setVisibility(accountId, visibility);
}

bool UsageStore::setTokenAccountSourceMode(const QString& accountId, int sourceMode)
{
    return m_tokenAccountManager->setSourceMode(accountId, sourceMode);
}

bool UsageStore::setDefaultTokenAccount(const QString& providerId, const QString& accountId)
{
    return m_tokenAccountManager->setDefault(providerId, accountId);
}

QString UsageStore::requestAddTokenAccount(const QString& providerId,
                                           const QString& displayName,
                                           int sourceMode)
{
    return m_tokenAccountManager->requestAddAccount(providerId, displayName, sourceMode, m_backend);
}

QString UsageStore::requestAddTokenAccountWithApiKey(const QString& providerId,
                                                     const QString& displayName,
                                                     int sourceMode,
                                                     const QString& apiKey)
{
    return m_tokenAccountManager->requestAddAccountWithApiKey(providerId, displayName, sourceMode, apiKey, m_backend);
}

QString UsageStore::requestRemoveTokenAccount(const QString& accountId)
{
    return m_tokenAccountManager->requestRemoveAccount(accountId, m_backend);
}

QString UsageStore::requestSetTokenAccountVisibility(const QString& accountId, int visibility)
{
    return m_tokenAccountManager->requestSetVisibility(accountId, visibility, m_backend);
}

QString UsageStore::requestSetTokenAccountSourceMode(const QString& accountId, int sourceMode)
{
    return m_tokenAccountManager->requestSetSourceMode(accountId, sourceMode, m_backend);
}

QString UsageStore::requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId)
{
    return m_tokenAccountManager->requestSetDefault(providerId, accountId, m_backend);
}

// ============================================================================
// BatchUpdateController slots
// ============================================================================

void UsageStore::onBatchUpdateReady(const QStringList& providerIds)
{
    PERF_PROBE("onBatchUpdateReady", 2000);

    // 逐个发射 snapshotChanged（QML 中 ProviderDetailView 等需要这个粒度）
    for (const auto& id : providerIds) {
        emit snapshotChanged(id);
    }

    for (const auto& id : providerIds) {
        m_uiService->invalidateSnapshotDataCache(id);
    }
    m_uiService->invalidateProviderListCache();
}

void UsageStore::onBatchFinished()
{
    emit refreshingChanged();
}

QString UsageStore::defaultTokenAccount(const QString& providerId) const
{
    return TokenAccountStore::instance()->defaultAccountId(providerId);
}
