#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <QVector>
#include <chrono>
#include <functional>

#include "UsageBackendJobs.h"
#include "UsageBackendTypes.h"
#include "../models/UsageSnapshot.h"
#include "../providers/ProviderFetchResult.h"

class UsageBackend;
class ProviderPipeline;
class IProvider;

/**
 * @brief Core refresh coordination: snapshot management, backend dispatch, refresh state tracking.
 *
 * Owns m_snapshots, m_errors, m_isRefreshing, m_pendingRefreshes, and the auto-refresh timer.
 * Cross-cutting concerns (history recording, session quota, Codex credits, dashboard data,
 * cache invalidation, BatchUpdater) remain in UsageStore and are handled via signal connections.
 *
 * Extracted from UsageStore during Phase 6 refactoring.
 */
class ProviderRefreshCoordinator : public QObject {
    Q_OBJECT
public:
    explicit ProviderRefreshCoordinator(QObject* parent = nullptr);
    ~ProviderRefreshCoordinator();

    // Refresh control
    void refresh(const QStringList& providerIds);
    void refreshProvider(const QString& providerId);
    void startAutoRefresh(int intervalMinutes);
    void stopAutoRefresh();

    // Snapshot access
    UsageSnapshot snapshot(const QString& providerId) const;
    QString error(const QString& providerId) const;
    QVariantMap dashboardData(const QString& providerId) const;
    QVector<ProviderFetchAttempt> fetchAttempts(const QString& providerId) const;
    bool isRefreshing() const { return m_isRefreshing; }
    int revision() const { return m_revision; }

    // Dependencies
    void setBackend(UsageBackend* backend);
    void setPipeline(ProviderPipeline* pipeline);

    // Result application (called from UsageStore::handleBackendResult)
    void applyRefreshResult(const QString& providerId, const ProviderFetchResult& result);
    void applyRefreshFailed(const QString& providerId, const QString& errorMessage);
    void removeSnapshot(const QString& providerId);

    // For connection tests and other non-refresh snapshot updates
    void applySnapshotUpdate(const QString& providerId, const ProviderFetchResult& result);
    void clearCache();
    bool handleBackendResult(const UsageBackendResult& result);

    // External async work tracking (e.g., Codex credits refresh)
    void incrementPendingExternalWork();
    void decrementPendingExternalWork();
    void setPendingExternalWork(int count);

    // Callbacks injected from UsageStore to avoid tight coupling
    using FetchCommandInputBuilder = std::function<UsageBackendJobs::ProviderFetchCommandInput(const QString&)>;
    using ProviderResolver = std::function<IProvider*(const QString&)>;

    void setFetchCommandInputBuilder(FetchCommandInputBuilder builder);
    void setProviderResolver(ProviderResolver resolver);

signals:
    void autoRefreshTriggered();
    void snapshotChanged(const QString& providerId);
    void revisionChanged();
    void refreshingChanged();
    void errorOccurred(const QString& providerId, const QString& message);
    void refreshStarted(const QStringList& providerIds);
    void refreshComplete();
    void providerRefreshSuccess(const QString& providerId, const ProviderFetchResult& result);
    void providerRefreshFailed(const QString& providerId, const QString& errorMessage);
    void credentialCacheUpdatesReady(const QVector<CredentialCacheUpdatePayload>& updates);
    void fetchAttemptsChanged(const QString& providerId);

private:
    void doRefresh(const QStringList& ids);
    void refreshWithBackend(const QString& providerId);
    void completeRefresh();
    void applyResult(const QString& providerId, const ProviderFetchResult& result, bool complete);
    void updateResultMetadata(const QString& providerId, const ProviderFetchResult& result);
    void clearResultMetadata(const QString& providerId);

    QTimer m_refreshTimer;
    QTimer m_refreshTimeoutTimer;
    static constexpr auto REFRESH_TIMEOUT = std::chrono::seconds(60);
    QHash<QString, UsageSnapshot> m_snapshots;
    QHash<QString, QString> m_errors;
    QHash<QString, QVariantMap> m_dashboardData;
    QHash<QString, QVector<ProviderFetchAttempt>> m_lastFetchAttempts;
    QHash<QString, QString> m_refreshRequestProviderIds;
    QSet<QString> m_pendingProviderIds;
    bool m_isRefreshing = false;
    int m_pendingRefreshes = 0;
    int m_pendingExternalWork = 0;
    int m_revision = 0;

    UsageBackend* m_backend = nullptr;
    ProviderPipeline* m_pipeline = nullptr;
    FetchCommandInputBuilder m_fetchCommandInputBuilder;
    ProviderResolver m_providerResolver;
};
