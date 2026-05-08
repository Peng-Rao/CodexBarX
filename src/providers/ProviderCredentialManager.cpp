#include "ProviderCredentialManager.h"
#include "shared/ProviderCredentialStore.h"
#include "../app/UsageBackend.h"
#include "../app/UsageBackendTypes.h"

#include <QMutexLocker>

ProviderCredentialManager::ProviderCredentialManager(QObject* parent)
    : QObject(parent)
{
}

QVariantMap ProviderCredentialManager::secretStatus(
    const QString& providerId,
    const QString& key,
    const std::optional<ProviderSettingsDescriptor>& descriptor,
    const QHash<QString, QString>& cachedEnv,
    const QVariant& settingsValue) const
{
    QVariantMap status;
    status[QStringLiteral("configured")] = false;
    status[QStringLiteral("source")] = QStringLiteral("none");

    if (!descriptor.has_value() || !descriptor->sensitive) return status;

    status[QStringLiteral("envVar")] = descriptor->envVar;
    status[QStringLiteral("credentialTarget")] = descriptor->credentialTarget;

    // Check env var
    if (!descriptor->envVar.isEmpty()) {
        if (cachedEnv.contains(descriptor->envVar)) {
            status[QStringLiteral("configured")] = true;
            status[QStringLiteral("source")] = QStringLiteral("env");
            status[QStringLiteral("readOnly")] = true;
            return status;
        }
    }

    if (!descriptor->credentialTarget.isEmpty()) {
        bool credentialExists = false;
        bool credentialCheckPending = false;
        {
            QMutexLocker locker(&m_cacheMutex);
            if (m_cache.contains(descriptor->credentialTarget)
                || m_existing.contains(descriptor->credentialTarget)) {
                credentialExists = true;
            } else if (!m_missing.contains(descriptor->credentialTarget)) {
                credentialCheckPending = true;
            }
        }

        if (credentialExists) {
            status[QStringLiteral("configured")] = true;
            status[QStringLiteral("source")] = QStringLiteral("credential");
            return status;
        }
        if (credentialCheckPending) {
            status[QStringLiteral("checking")] = true;
            status[QStringLiteral("source")] = QStringLiteral("checking");
            // Caller should call queueCredentialStatusCheck()
            return status;
        }
    }

    // Known-missing credentials fall through
    if (!descriptor->credentialTarget.isEmpty()) {
        return status;
    }

    if (settingsValue.isValid() && !settingsValue.toString().isEmpty()) {
        status[QStringLiteral("configured")] = true;
        status[QStringLiteral("source")] = QStringLiteral("settings");
    }
    return status;
}

bool ProviderCredentialManager::hasCredential(const QString& target) const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_cache.contains(target) || m_existing.contains(target);
}

bool ProviderCredentialManager::isCredentialMissing(const QString& target) const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_missing.contains(target);
}

bool ProviderCredentialManager::isCredentialCheckPending(const QString& target) const
{
    QMutexLocker locker(&m_cacheMutex);
    return m_statusInFlight.contains(target);
}

void ProviderCredentialManager::preloadCredentials(
    const QVector<UsageBackendJobs::CredentialPreloadItem>& items,
    UsageBackend* backend)
{
    if (items.isEmpty() || !backend) return;

    backend->dispatchValueJob(QStringLiteral("credentialPreload"), 0,
        [items]() -> QVariant {
            return QVariant::fromValue(UsageBackendJobs::preloadCredentials(items));
        });
}

void ProviderCredentialManager::applyCacheUpdates(const QVector<CredentialCacheUpdatePayload>& updates)
{
    if (updates.isEmpty()) return;

    QMutexLocker locker(&m_cacheMutex);
    for (const auto& update : updates) {
        if (update.target.isEmpty()) continue;
        if (update.exists) {
            m_cache[update.target] = {update.data, QDateTime::currentDateTime()};
            m_existing.insert(update.target);
            m_missing.remove(update.target);
        } else {
            m_cache.remove(update.target);
            m_existing.remove(update.target);
            m_missing[update.target] = true;
        }
        m_statusInFlight.remove(update.target);
    }
}

void ProviderCredentialManager::queueCredentialStatusCheck(
    const QString& providerId,
    const QString& key,
    const QString& target,
    UsageBackend* backend)
{
    if (target.isEmpty() || !backend) return;

    {
        QMutexLocker locker(&m_cacheMutex);
        if (m_cache.contains(target)
            || m_existing.contains(target)
            || m_missing.contains(target)
            || m_statusInFlight.contains(target)) {
            return;
        }
        m_statusInFlight.insert(target);
    }

    backend->dispatchValueJob(QStringLiteral("credentialStatusCheck"), 0,
        [providerId, key, target]() -> QVariant {
            CredentialStatusPayload payload;
            payload.providerId = providerId;
            payload.key = key;
            payload.target = target;
            payload.exists = ProviderCredentialStore::exists(target);
            return QVariant::fromValue(payload);
        });
}

QString ProviderCredentialManager::setSecret(
    const QString& providerId,
    const QString& key,
    const QString& value,
    const QString& target,
    UsageBackend* backend)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || target.isEmpty() || !backend) {
        return {};
    }

    const QByteArray secret = trimmed.toUtf8();
    const UsageBackendRequest request = backend->dispatchValueJob(
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

    m_pendingSecretValues.insert(request.requestId, secret);
    return request.requestId;
}

QString ProviderCredentialManager::clearSecret(
    const QString& providerId,
    const QString& key,
    const QString& target,
    UsageBackend* backend)
{
    if (target.isEmpty() || !backend) {
        return {};
    }

    const UsageBackendRequest request = backend->dispatchValueJob(
        QStringLiteral("providerSecretRemove"), 0,
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

    return request.requestId;
}

void ProviderCredentialManager::applyCredentialStatusResult(const CredentialStatusPayload& payload)
{
    {
        QMutexLocker locker(&m_cacheMutex);
        m_statusInFlight.remove(payload.target);
        if (payload.exists) {
            m_existing.insert(payload.target);
            m_missing.remove(payload.target);
        } else {
            m_existing.remove(payload.target);
            m_missing[payload.target] = true;
        }
    }
    emit secretChanged(payload.providerId, payload.key);
}

void ProviderCredentialManager::applySecretResult(
    const ProviderSecretResultPayload& payload,
    const QByteArray& secret)
{
    if (payload.success) {
        QMutexLocker locker(&m_cacheMutex);
        if (payload.removed) {
            m_cache.remove(payload.target);
            m_existing.remove(payload.target);
            m_missing[payload.target] = true;
        } else {
            m_cache[payload.target] = {secret, QDateTime::currentDateTime()};
            m_existing.insert(payload.target);
            m_missing.remove(payload.target);
        }
        m_statusInFlight.remove(payload.target);
    }
    emit secretChanged(payload.providerId, payload.key);
}

void ProviderCredentialManager::populateCredentialCacheInput(
    const QString& target,
    UsageBackendJobs::CredentialCacheInput& cache) const
{
    cache.target = target;
    QMutexLocker locker(&m_cacheMutex);
    auto cacheIt = m_cache.find(target);
    if (cacheIt != m_cache.end()
        && cacheIt.value().cachedAt.msecsTo(QDateTime::currentDateTime()) < CACHE_TTL_MS) {
        cache.hasValue = true;
        cache.value = cacheIt.value().data;
    }
    cache.missing = m_missing.contains(target);
}

std::optional<QByteArray> ProviderCredentialManager::getCachedCredential(
    const QString& target,
    bool allowReadFromStore)
{
    bool cacheHit = false;
    bool cacheExpired = true;
    bool isMissing = false;
    QByteArray result;

    {
        QMutexLocker locker(&m_cacheMutex);
        auto cacheIt = m_cache.find(target);
        if (cacheIt != m_cache.end()) {
            cacheHit = true;
            cacheExpired = cacheIt.value().cachedAt.msecsTo(QDateTime::currentDateTime()) >= CACHE_TTL_MS;
            if (!cacheExpired) {
                result = cacheIt.value().data;
            }
        }
        isMissing = m_missing.contains(target);
    }

    if (!cacheHit && !isMissing && allowReadFromStore) {
        auto stored = ProviderCredentialStore::read(target);
        QMutexLocker locker(&m_cacheMutex);
        if (stored.has_value()) {
            result = stored.value();
            m_cache[target] = {stored.value(), QDateTime::currentDateTime()};
        } else {
            m_missing[target] = true;
        }
    }

    if (result.isEmpty()) {
        return std::nullopt;
    }
    return result;
}

void ProviderCredentialManager::markCredentialMissing(const QString& target)
{
    QMutexLocker locker(&m_cacheMutex);
    m_missing[target] = true;
}

void ProviderCredentialManager::clearCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_cache.clear();
    m_missing.clear();
}

void ProviderCredentialManager::handleBackendResult(
    const QString& kind,
    const QVariant& payload,
    const QString& requestId)
{
    if (kind == QLatin1String("credentialStatusCheck")) {
        const auto p = payload.value<CredentialStatusPayload>();
        applyCredentialStatusResult(p);
    } else if (kind == QLatin1String("credentialPreload")) {
        const auto p = payload.value<CredentialPreloadPayload>();
        applyCacheUpdates(p.updates);
    } else if (kind == QLatin1String("providerSecretWrite")
               || kind == QLatin1String("providerSecretRemove")) {
        const auto p = payload.value<ProviderSecretResultPayload>();
        const QByteArray secret = m_pendingSecretValues.take(requestId);
        applySecretResult(p, secret);
    }
}

QByteArray ProviderCredentialManager::takePendingSecretValue(const QString& requestId)
{
    return m_pendingSecretValues.take(requestId);
}
