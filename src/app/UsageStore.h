#pragma once

#include <QObject>
#include <QHash>
#include <QSharedPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QAtomicInt>
#include <optional>
#include "../models/UsageSnapshot.h"
#include "../models/CreditsSnapshot.h"
#include "../models/CostUsageReport.h"
#include "SessionQuotaNotifications.h"
#include "../providers/IFetchStrategy.h"
#include "../providers/ProviderFetchContext.h"
#include "../providers/ProviderFetchResult.h"
#include "../providers/ProviderCatalogSnapshot.h"
#include "../providers/codex/CodexConsumerProjection.h"
#include "../providers/codex/CodexCreditsFetcher.h"
#include "UsageBackendJobs.h"

class ProviderRegistry;
class ProviderPipeline;
class SettingsStore;
class PlanUtilizationHistoryStore;
class BatchUpdateController;
class UsageBackend;
class ProviderCredentialManager;
class ProviderStatusManager;
struct UsageBackendResult;
struct ProviderLoginStartPayload;

class UsageStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY refreshingChanged)
    Q_PROPERTY(QStringList providerIDs READ providerIDs NOTIFY providerIDsChanged)
    Q_PROPERTY(bool costUsageEnabled READ costUsageEnabled WRITE setCostUsageEnabled NOTIFY costUsageEnabledChanged)
    Q_PROPERTY(bool costUsageRefreshing READ costUsageRefreshing NOTIFY costUsageRefreshingChanged)
    Q_PROPERTY(int snapshotRevision READ snapshotRevision NOTIFY snapshotRevisionChanged)
    Q_PROPERTY(int statusRevision READ statusRevision NOTIFY statusRevisionChanged)
    Q_PROPERTY(QVariantMap codexAccountState READ codexAccountState NOTIFY codexAccountStateChanged)
    Q_PROPERTY(QVariantList codexFetchAttempts READ codexFetchAttempts NOTIFY codexFetchAttemptsChanged)
    Q_PROPERTY(QString lastKnownSessionWindowSource READ lastKnownSessionWindowSource NOTIFY lastKnownSessionWindowSourceChanged)
    Q_PROPERTY(QVariantMap tokenAccountOperationState READ tokenAccountOperationState NOTIFY tokenAccountOperationStateChanged)

public:
    explicit UsageStore(QObject* parent = nullptr);

    Q_INVOKABLE UsageSnapshot snapshot(const QString& providerId) const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshAll();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void refreshProvider(const QString& providerId);
    void setProviderEnabled(const QString& id, bool enabled);
    Q_INVOKABLE bool isProviderEnabled(const QString& id) const;
    Q_INVOKABLE QString providerDisplayName(const QString& id) const;
    Q_INVOKABLE QVariantMap snapshotData(const QString& id) const;
    Q_INVOKABLE QString providerError(const QString& id) const;
    Q_INVOKABLE QVariantList providerList() const;
    Q_INVOKABLE void moveProvider(int fromIndex, int toIndex);
    Q_INVOKABLE QVariantMap providerDescriptorData(const QString& id) const;
    Q_INVOKABLE void requestProviderList();
    Q_INVOKABLE void requestProviderDescriptor(const QString& providerId);
    void setProviderSetting(const QString& providerId, const QString& key, const QVariant& value);
    Q_INVOKABLE QVariantMap providerSecretStatus(const QString& providerId, const QString& key) const;
    bool setProviderSecret(const QString& providerId, const QString& key, const QString& value);
    bool clearProviderSecret(const QString& providerId, const QString& key);
    void testProviderConnection(const QString& providerId);
    Q_INVOKABLE QVariantMap providerConnectionTest(const QString& providerId) const;
    void startProviderLogin(const QString& providerId);
    void cancelProviderLogin(const QString& providerId);
    Q_INVOKABLE QVariantMap providerLoginState(const QString& providerId) const;
    void refreshProviderStatuses();
    Q_INVOKABLE QVariantMap providerStatus(const QString& providerId) const;
    Q_INVOKABLE QString providerStatusURL(const QString& providerId) const;
    Q_INVOKABLE QVariantMap providerUsageSnapshot(const QString& providerId) const;
    Q_INVOKABLE QStringList allProviderIDs() const;

    Q_INVOKABLE void refreshCostUsage();
    Q_INVOKABLE void ensureCostUsageEnabled();
    Q_INVOKABLE void requestCostUsageViewData();
    Q_INVOKABLE void requestCostUsageSummary() const;
    Q_INVOKABLE void requestCostUsageProviderRows() const;
    Q_INVOKABLE void requestCostUsageDetailsRows() const;
    Q_INVOKABLE void releaseCostUsageViewCaches() const;
    Q_INVOKABLE QVariantMap costUsageData() const;
    Q_INVOKABLE QVariantList providerCostUsageList() const;
    QVariantList costUsageDetailsRows() const;
    int costUsageTokenProviderCount() const;
    QVariantMap costUsageProviderDetail(const QString& providerId) const;
    void requestCostUsageProviderDetail(const QString& providerId) const;

    Q_INVOKABLE QVariantList utilizationChartData(const QString& providerId, const QString& seriesName) const;
    Q_INVOKABLE QVariantList codexFetchAttempts() const;
    Q_INVOKABLE QVariantMap providerDashboardData(const QString& providerId) const;
    QVariantMap codexConsumerProjectionData() const;
    QString lastKnownSessionWindowSource() const;

    // Codex multi-account management
    Q_INVOKABLE QVariantList codexAccounts() const;
    Q_INVOKABLE QVariantMap codexAccountState() const;
    Q_INVOKABLE QString codexActiveAccountID() const;
    Q_INVOKABLE void setCodexActiveAccount(const QString& accountID);
    Q_INVOKABLE bool addCodexAccount(const QString& email, const QString& homePath);
    Q_INVOKABLE void cancelCodexAuthentication();
    Q_INVOKABLE bool removeCodexAccount(const QString& accountID);
    Q_INVOKABLE bool reauthenticateCodexAccount(const QString& accountID);
    Q_INVOKABLE bool promoteCodexAccount(const QString& accountID);
    Q_INVOKABLE bool isCodexAuthenticating() const;

    // Token Account management (generic multi-account support)
    Q_INVOKABLE QVariantList tokenAccountsForProvider(const QString& providerId) const;
    Q_INVOKABLE QString addTokenAccount(const QString& providerId, const QString& displayName, int sourceMode);
    Q_INVOKABLE QString addTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey);
    Q_INVOKABLE bool removeTokenAccount(const QString& accountId);
    Q_INVOKABLE bool setTokenAccountVisibility(const QString& accountId, int visibility);
    Q_INVOKABLE bool setTokenAccountSourceMode(const QString& accountId, int sourceMode);
    Q_INVOKABLE bool setDefaultTokenAccount(const QString& providerId, const QString& accountId);
    Q_INVOKABLE QString defaultTokenAccount(const QString& providerId) const;
    Q_INVOKABLE QVariantMap tokenAccountOperationState() const;
    Q_INVOKABLE QString requestAddTokenAccount(const QString& providerId, const QString& displayName, int sourceMode);
    Q_INVOKABLE QString requestAddTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey);
    Q_INVOKABLE QString requestRemoveTokenAccount(const QString& accountId);
    Q_INVOKABLE QString requestSetTokenAccountVisibility(const QString& accountId, int visibility);
    Q_INVOKABLE QString requestSetTokenAccountSourceMode(const QString& accountId, int sourceMode);
    Q_INVOKABLE QString requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId);

    void shutdown();
    Q_INVOKABLE bool isCodexRemoving() const;
    Q_INVOKABLE QString codexAuthenticatingAccountID() const;
    Q_INVOKABLE QString codexRemovingAccountID() const;
    Q_INVOKABLE bool hasCodexUnreadableStore() const;

    bool costUsageEnabled() const { return m_costUsageEnabled; }
    void setCostUsageEnabled(bool v);
    bool costUsageRefreshing() const { return m_costUsageRefreshing; }

    QStringList providerIDs() const;
    void updateProviderIDs();

    void startAutoRefresh(int intervalMinutes);
    void stopAutoRefresh();
    bool isRefreshing() const { return m_isRefreshing; }
    int snapshotRevision() const { return m_snapshotRevision; }
    int statusRevision() const { return m_statusRevision; }
    QString error(const QString& providerId) const;
    ProviderFetchContext buildFetchContextForProvider(const QString& providerId) const;

    void setSettingsStore(SettingsStore* s);

    // Preload all provider credentials into cache (runs on background thread)
    void preloadCredentials();
    void requestPreloadCredentials();

    // Rebuild the cached snapshot of system environment variables.
    // Call after qputenv() in tests so that the cache picks up the new values.
    static void rebuildSystemEnvCache();

signals:
    void snapshotChanged(const QString& providerId);
    void refreshingChanged();
    void providerIDsChanged();
    void errorOccurred(const QString& providerId, const QString& message);
    void costUsageEnabledChanged();
    void costUsageRefreshingChanged();
    void costUsageChanged();
    void costUsageProviderDetailChanged(const QString& providerId);
    void snapshotRevisionChanged();
    void providerConnectionTestChanged(const QString& providerId);
    void providerLoginStateChanged(const QString& providerId);
    void providerStatusChanged(const QString& providerId);
    void providerSecretChanged(const QString& providerId, const QString& key);
    void providerListModelChanged();
    void providerDescriptorChanged(const QString& providerId);
    void tokenAccountsChanged(const QString& providerId);
    void tokenAccountOperationStateChanged();
    void tokenAccountOperationFinished(const QString& operationId,
                                       const QString& providerId,
                                       bool success,
                                       const QString& message);
    void statusRevisionChanged();

    // Codex multi-account signals
    void codexAccountsChanged();
    void codexActiveAccountChanged(const QString& accountID);
    void codexAuthenticationStarted(const QString& accountID);
    void codexAuthenticationFinished(const QString& accountID, bool success);
    void codexRemovalStarted(const QString& accountID);
    void codexRemovalFinished(const QString& accountID, bool success);
    void codexAccountStateChanged();

    // Codex credits signals
    void codexCreditsChanged();
    void codexFetchAttemptsChanged();
    void lastKnownSessionWindowSourceChanged();

private:
    struct CodexAccountRefreshGuard {
        QString source;  // "liveSystem" or "managedAccount"
        QString identity;
        QString accountKey;

        bool operator==(const CodexAccountRefreshGuard& other) const {
            return source == other.source && identity == other.identity && accountKey == other.accountKey;
        }
        bool operator!=(const CodexAccountRefreshGuard& other) const {
            return !(*this == other);
        }
        bool isEmpty() const {
            return source.isEmpty() && identity.isEmpty() && accountKey.isEmpty();
        }
    };

    std::optional<ProviderSettingsDescriptor> settingDescriptor(const QString& providerId,
                                                                const QString& key) const;
    void rebuildProviderCatalogSnapshot();
    QVariantList buildProviderListNow() const;
    QVariantMap buildProviderDescriptorDataNow(const QString& id) const;
    QSet<QString> costUsageSubscribedProviderIDs() const;
    void setProviderLoginState(const QString& providerId, const QVariantMap& state);
    void setProviderConnectionTest(const QString& providerId, const QVariantMap& state);
    void setProviderStatus(const QString& providerId, const QVariantMap& status);
    void setProviderStatuses(const QHash<QString, QVariantMap>& statuses);
    void refreshProviderWithBackend(const QString& providerId);
    void applyProviderRefreshResult(const QString& providerId, const ProviderFetchResult& result);
    void completeProviderRefresh();
    void applyProviderConnectionTestResult(const QString& providerId,
                                           const ProviderFetchResult& result,
                                           qint64 startedAt);
    void handleBackendResult(const UsageBackendResult& result);
    void configureStatusPolling();
    void queueCredentialStatusCheck(const QString& providerId, const QString& key, const QString& target) const;
    QHash<QString, QString> codexCreditsEnvironment() const;
    void dispatchCodexCreditsRefresh(const QHash<QString, QString>& env,
                                     const CodexAccountRefreshGuard& expectedGuard);
    void dispatchProviderLoginPoll(const ProviderLoginStartPayload& startPayload,
                                   const QSharedPointer<QAtomicInt>& cancelFlag);
    UsageBackendJobs::ProviderFetchCommandInput buildProviderFetchCommandInput(const QString& providerId) const;
    QVector<UsageBackendJobs::CredentialPreloadItem> buildCredentialPreloadItems() const;
    void applyCredentialCacheUpdates(const QVector<CredentialCacheUpdatePayload>& updates);

    QTimer m_refreshTimer;
    QTimer m_statusTimer;
    QHash<QString, UsageSnapshot> m_snapshots;
    QHash<QString, QString> m_errors;
    QHash<QString, QVariantMap> m_connectionTests;
    QHash<QString, QVariantMap> m_loginStates;
    QHash<QString, QVariantMap> m_providerStatuses;
    QHash<QString, QSharedPointer<QAtomicInt>> m_loginCancelFlags;
    QHash<QString, std::optional<double>> m_lastKnownSessionRemaining;
    QHash<QString, QVector<ProviderFetchAttempt>> m_lastFetchAttempts;
    QHash<QString, QVariantMap> m_dashboardData;
    QString m_lastKnownSessionWindowSource;
    QStringList m_providerIDs;
    bool m_isRefreshing = false;

    bool m_costUsageEnabled = false;
    bool m_costUsageRefreshing = false;
    CostUsageSnapshot m_costUsage;
    QHash<QString, CostUsageSnapshot> m_perProviderCostUsage;
    QVector<ProviderCostUsageSnapshot> m_allProviderCostUsage;
    UsageBackend* m_backend = nullptr;
    ProviderPipeline* m_pipeline = nullptr;
    SettingsStore* m_settingsStore = nullptr;
    ProviderCatalogSnapshot m_providerCatalog;
    int m_providerCatalogGeneration = 0;
    PlanUtilizationHistoryStore* m_historyStore = nullptr;
    int m_pendingRefreshes = 0;
    int m_snapshotRevision = 0;
    int m_statusRevision = 0;
    bool m_batchRefreshInProgress = false;

    // Codex multi-account
    class ManagedCodexAccountService* m_codexAccountService = nullptr;

    // Credential manager (Phase 1 extraction)
    ProviderCredentialManager* m_credentialManager = nullptr;

    // Status manager (Phase 2 extraction)
    ProviderStatusManager* m_statusManager = nullptr;

    // Credential cache to avoid blocking main thread with WinCred API calls
    struct CredentialEntry {
        QByteArray data;
        QDateTime cachedAt;
    };
    mutable QMutex m_credentialCacheMutex;
    mutable QHash<QString, CredentialEntry> m_credentialCache;
    mutable QHash<QString, bool> m_credentialMissing;
    mutable QSet<QString> m_credentialExisting;
    mutable QSet<QString> m_credentialStatusInFlight;
    static constexpr int CREDENTIAL_CACHE_TTL_MS = 300000; // 5 minutes

    // snapshotData() result cache — invalidated on snapshotChanged / snapshotRevisionChanged
    mutable QHash<QString, QVariantMap> m_snapshotDataCache;

    // costUsageData() / providerCostUsageList() result caches — invalidated on costUsageChanged
    mutable QVariantMap m_costUsageDataCache;
    mutable QVariantList m_providerCostUsageListCache;
    mutable QVariantList m_costUsageDetailsRowsCache;
    mutable int m_costUsageTokenProviderCountCache = 0;
    mutable QHash<QString, QVariantMap> m_costUsageProviderDetailCache;
    mutable QSet<QString> m_costUsageProviderDetailQueued;
    mutable bool m_costUsageDataCacheValid = false;
    mutable bool m_providerCostUsageListCacheValid = false;
    mutable bool m_costUsageDetailsRowsCacheValid = false;
    mutable bool m_costUsageSummaryBuildQueued = false;
    mutable bool m_costUsageProviderRowsBuildQueued = false;
    mutable bool m_costUsageDetailsRowsBuildQueued = false;
    mutable int m_costUsageSummaryBuildGeneration = 0;
    mutable int m_costUsageProviderRowsBuildGeneration = 0;
    mutable int m_costUsageDetailsRowsBuildGeneration = 0;
    mutable int m_costUsageProviderDetailBuildGeneration = 0;
    int m_costUsageRefreshGeneration = 0;

    // providerList() result cache — invalidated on providerIDsChanged / snapshotRevisionChanged / statusRevisionChanged
    mutable QVariantList m_providerListCache;
    mutable bool m_providerListCacheValid = false;
    mutable bool m_providerListRefreshQueued = false;
    int m_providerListRefreshGeneration = 0;
    mutable QHash<QString, QVariantMap> m_providerDescriptorDataCache;
    mutable QSet<QString> m_providerDescriptorRefreshQueued;
    QHash<QString, int> m_providerDescriptorRefreshGenerations;
    int m_providerStatusRefreshGeneration = 0;
    QHash<QString, QString> m_backendRequestProviderIds;
    QHash<QString, CodexAccountRefreshGuard> m_backendCodexCreditGuards;
    QHash<QString, QByteArray> m_backendSecretValues;

    // Codex credits cache
    struct CodexCreditsCache {
        std::optional<CreditsSnapshot> snapshot;
        QString accountKey;
        QDateTime updatedAt;
        int failureStreak = 0;
        QString lastError;
    };
    CodexCreditsCache m_codexCreditsCache;

    CodexAccountRefreshGuard m_lastCodexRefreshGuard;

    CodexAccountRefreshGuard currentCodexAccountRefreshGuard() const;
    bool shouldApplyCodexScopedNonUsageResult(const CodexAccountRefreshGuard& expectedGuard) const;

    // Codex credits methods
    void refreshCodexCredits(const CodexAccountRefreshGuard& expectedGuard = {});
    void applyCodexCreditsFetchResult(const CodexCreditsFetcher::FetchResult& result,
                                       const CodexAccountRefreshGuard& expectedGuard = {});
    QString currentCodexAccountKey() const;
    std::optional<CreditsSnapshot> cachedCodexCredits() const;
    QString codexCreditsError() const;
    bool codexCreditsRefreshing() const { return m_codexCreditsRefreshing; }

    // Wait for codex snapshot to be at least as fresh as minimumUpdatedAt (mirrors original CodexBar)
    UsageSnapshot waitForCodexSnapshot(const QDateTime& minimumUpdatedAt, int timeoutMs = 6000) const;

    void clearCodexOpenAIWebState();
    void doRefresh(const QStringList& ids);
    // Batch update controller (merges UI signals to avoid signal storm)
    BatchUpdateController* m_batchUpdater = nullptr;

    void onBatchUpdateReady(const QStringList& providerIds);
    void onBatchFinished();
    QString beginTokenAccountOperation(const QString& kind, const QString& providerId, const QString& targetId);
    void finishTokenAccountOperation(const QString& operationId,
                                     const QString& providerId,
                                     bool success,
                                     const QString& message,
                                     bool refreshProviderOnSuccess);
    void rebuildTokenAccountOperationState();
    void saveTokenAccountStoreAsync(const QString& operationId,
                                    const QString& providerId,
                                    bool refreshProviderOnSuccess);

    bool m_codexCreditsRefreshing = false;
    int m_pendingCreditsRefresh = 0;
    QHash<QString, QVariantMap> m_tokenAccountOperations;
    QVariantMap m_tokenAccountOperationState;

    // Test injection point for credits fetching (mirrors original _test_codexCreditsLoaderOverride)
    std::function<std::optional<CreditsSnapshot>()> _test_codexCreditsLoaderOverride;
};
