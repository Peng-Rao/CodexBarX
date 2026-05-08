#pragma once

#include "IFetchStrategy.h"
#include "../app/UsageBackendJobs.h"

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QSet>
#include <QString>
#include <QVariantMap>
#include <QDateTime>
#include <functional>

class UsageBackend;
struct ProviderCatalogEntry;
struct CredentialCacheUpdatePayload;
struct CredentialStatusPayload;
struct ProviderSecretResultPayload;

class ProviderCredentialManager : public QObject {
    Q_OBJECT
public:
    explicit ProviderCredentialManager(QObject* parent = nullptr);

    // Credential status query (thread-safe)
    QVariantMap secretStatus(
        const QString& providerId,
        const QString& key,
        const std::optional<ProviderSettingsDescriptor>& descriptor,
        const QHash<QString, QString>& cachedEnv,
        const QVariant& settingsValue) const;

    // Check if credential exists in cache (thread-safe)
    bool hasCredential(const QString& target) const;
    bool isCredentialMissing(const QString& target) const;
    bool isCredentialCheckPending(const QString& target) const;

    // Cache operations
    void preloadCredentials(
        const QVector<UsageBackendJobs::CredentialPreloadItem>& items,
        UsageBackend* backend);

    void applyCacheUpdates(const QVector<CredentialCacheUpdatePayload>& updates);

    // Async credential status check (queues backend job)
    void queueCredentialStatusCheck(
        const QString& providerId,
        const QString& key,
        const QString& target,
        UsageBackend* backend);

    // Secret write/remove operations (async via backend)
    QString setSecret(
        const QString& providerId,
        const QString& key,
        const QString& value,
        const QString& target,
        UsageBackend* backend);

    QString clearSecret(
        const QString& providerId,
        const QString& key,
        const QString& target,
        UsageBackend* backend);

    // Apply results from backend jobs
    void applyCredentialStatusResult(const CredentialStatusPayload& payload);
    void applySecretResult(const ProviderSecretResultPayload& payload, const QByteArray& secret);

    // Build credential cache input for provider fetch
    void populateCredentialCacheInput(
        const QString& target,
        UsageBackendJobs::CredentialCacheInput& cache) const;

    // Get cached credential for fetch context (may read from store if cache miss)
    std::optional<QByteArray> getCachedCredential(
        const QString& target,
        bool allowReadFromStore = true);

    // Mark credential as missing
    void markCredentialMissing(const QString& target);

    // Clear all caches
    void clearCache();

    // Handle backend result
    void handleBackendResult(const QString& kind, const QVariant& payload, const QString& requestId);

    // Take pending secret value for a request (used by UsageStore)
    QByteArray takePendingSecretValue(const QString& requestId);

signals:
    void secretChanged(const QString& providerId, const QString& key);

private:
    struct CredentialEntry {
        QByteArray data;
        QDateTime cachedAt;
    };

    mutable QMutex m_cacheMutex;
    mutable QHash<QString, CredentialEntry> m_cache;
    mutable QHash<QString, bool> m_missing;
    mutable QSet<QString> m_existing;
    mutable QSet<QString> m_statusInFlight;

    QHash<QString, QByteArray> m_pendingSecretValues;

    static constexpr int CACHE_TTL_MS = 300000; // 5 minutes
};
