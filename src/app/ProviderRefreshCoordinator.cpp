#include "ProviderRefreshCoordinator.h"

#include "UsageBackend.h"
#include "UsageBackendTypes.h"
#include "../providers/IProvider.h"

#include <QDebug>
#include <QVariant>

ProviderRefreshCoordinator::ProviderRefreshCoordinator(QObject* parent)
    : QObject(parent)
{
    m_refreshTimer.setSingleShot(false);
    QObject::connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
        emit refreshingChanged();
    });
}

// ============================================================================
// Refresh control
// ============================================================================

void ProviderRefreshCoordinator::refresh(const QStringList& providerIds)
{
    doRefresh(providerIds);
}

void ProviderRefreshCoordinator::refreshProvider(const QString& providerId)
{
    // Ensure count is sane when called individually (outside doRefresh).
    if (m_pendingRefreshes <= 0) {
        if (!m_isRefreshing) {
            m_isRefreshing = true;
            emit refreshingChanged();
        }
    }
    refreshWithBackend(providerId);
}

void ProviderRefreshCoordinator::startAutoRefresh(int intervalMinutes)
{
    if (intervalMinutes <= 0) {
        m_refreshTimer.stop();
        return;
    }
    m_refreshTimer.setInterval(intervalMinutes * 60 * 1000);
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start();
    }
}

void ProviderRefreshCoordinator::stopAutoRefresh()
{
    m_refreshTimer.stop();
}

// ============================================================================
// Snapshot access
// ============================================================================

UsageSnapshot ProviderRefreshCoordinator::snapshot(const QString& providerId) const
{
    return m_snapshots.value(providerId);
}

QString ProviderRefreshCoordinator::error(const QString& providerId) const
{
    return m_errors.value(providerId);
}

QVariantMap ProviderRefreshCoordinator::dashboardData(const QString& providerId) const
{
    return m_dashboardData.value(providerId);
}

QVector<ProviderFetchAttempt> ProviderRefreshCoordinator::fetchAttempts(const QString& providerId) const
{
    return m_lastFetchAttempts.value(providerId);
}

// ============================================================================
// Dependencies
// ============================================================================

void ProviderRefreshCoordinator::setBackend(UsageBackend* backend)
{
    m_backend = backend;
}

void ProviderRefreshCoordinator::setFetchCommandInputBuilder(FetchCommandInputBuilder builder)
{
    m_fetchCommandInputBuilder = std::move(builder);
}

void ProviderRefreshCoordinator::setProviderResolver(ProviderResolver resolver)
{
    m_providerResolver = std::move(resolver);
}

// ============================================================================
// Result application
// ============================================================================

void ProviderRefreshCoordinator::applyRefreshResult(const QString& providerId,
                                                     const ProviderFetchResult& result)
{
    applyResult(providerId, result, true);
}

void ProviderRefreshCoordinator::applyRefreshFailed(const QString& providerId,
                                                     const QString& errorMessage)
{
    clearResultMetadata(providerId);
    m_errors[providerId] = errorMessage;
    emit providerRefreshFailed(providerId, errorMessage);
    emit errorOccurred(providerId, errorMessage);
    emit snapshotChanged(providerId);
    completeRefresh();
}

void ProviderRefreshCoordinator::removeSnapshot(const QString& providerId)
{
    m_snapshots.remove(providerId);
    m_errors.remove(providerId);
    clearResultMetadata(providerId);
}

void ProviderRefreshCoordinator::applySnapshotUpdate(const QString& providerId,
                                                      const ProviderFetchResult& result)
{
    applyResult(providerId, result, false);
}

void ProviderRefreshCoordinator::clearCache()
{
    m_snapshots.clear();
    m_errors.clear();
    m_dashboardData.clear();
    m_lastFetchAttempts.clear();
    m_revision++;
    emit revisionChanged();
}

bool ProviderRefreshCoordinator::handleBackendResult(const UsageBackendResult& result)
{
    if (result.kind != QLatin1String("providerRefresh")) {
        return false;
    }

    const QString requestProviderId = m_refreshRequestProviderIds.take(result.requestId);
    if (!result.success) {
        qWarning() << "Provider refresh backend job failed:" << requestProviderId << result.message;
        if (!requestProviderId.isEmpty()) {
            applyRefreshFailed(requestProviderId, result.message);
        }
        return true;
    }

    const auto payload = result.payload.value<ProviderRefreshPayload>();
    emit credentialCacheUpdatesReady(payload.credentialUpdates);
    applyRefreshResult(payload.providerId, payload.fetchResult);
    return true;
}

void ProviderRefreshCoordinator::incrementPendingExternalWork()
{
    m_pendingExternalWork++;
}

void ProviderRefreshCoordinator::decrementPendingExternalWork()
{
    m_pendingExternalWork--;
    Q_ASSERT(m_pendingExternalWork >= 0);
    if (m_pendingExternalWork < 0) {
        m_pendingExternalWork = 0;
    }
}

void ProviderRefreshCoordinator::setPendingExternalWork(int count)
{
    m_pendingExternalWork = count;
}

// ============================================================================
// Private refresh helpers
// ============================================================================

void ProviderRefreshCoordinator::doRefresh(const QStringList& ids)
{
    if (m_isRefreshing) return;
    m_isRefreshing = true;
    emit refreshingChanged();
    emit refreshStarted(ids);

    if (ids.isEmpty()) {
        m_isRefreshing = false;
        emit refreshComplete();
        emit refreshingChanged();
        return;
    }

    for (const auto& id : ids) {
        refreshWithBackend(id);
    }
}

void ProviderRefreshCoordinator::refreshWithBackend(const QString& providerId)
{
    m_pendingRefreshes++;

    if (!m_backend) {
        completeRefreshDispatchWithoutResult();
        return;
    }

    IProvider* provider = m_providerResolver ? m_providerResolver(providerId) : nullptr;
    if (!provider) {
        completeRefreshDispatchWithoutResult();
        return;
    }

    UsageBackendJobs::ProviderFetchCommandInput input;
    if (m_fetchCommandInputBuilder) {
        input = m_fetchCommandInputBuilder(providerId);
    }

    const UsageBackendRequest request = m_backend->dispatchValueJob(
        QStringLiteral("providerRefresh"), 0, [provider, input]() {
        return QVariant::fromValue(UsageBackendJobs::refreshProvider(provider, input));
    });

    m_refreshRequestProviderIds.insert(request.requestId, providerId);
}

void ProviderRefreshCoordinator::completeRefresh()
{
    m_pendingRefreshes--;
    if (m_pendingRefreshes <= 0 && m_pendingExternalWork <= 0) {
        m_isRefreshing = false;
        emit refreshComplete();
        emit refreshingChanged();
    }
}

void ProviderRefreshCoordinator::completeRefreshDispatchWithoutResult()
{
    m_pendingRefreshes--;
    if (m_pendingRefreshes <= 0) {
        m_isRefreshing = false;
        emit refreshComplete();
        emit refreshingChanged();
    }
}

void ProviderRefreshCoordinator::applyResult(const QString& providerId,
                                             const ProviderFetchResult& result,
                                             bool complete)
{
    if (result.success) {
        updateResultMetadata(providerId, result);
        m_snapshots[providerId] = result.usage;
        m_errors.remove(providerId);
        emit providerRefreshSuccess(providerId, result);
        emit snapshotChanged(providerId);
        m_revision++;
        emit revisionChanged();
    } else {
        clearResultMetadata(providerId);
        m_errors[providerId] = result.errorMessage;
        emit providerRefreshFailed(providerId, result.errorMessage);
        emit errorOccurred(providerId, result.errorMessage);
        emit snapshotChanged(providerId);
    }

    if (complete) {
        completeRefresh();
    }
}

void ProviderRefreshCoordinator::updateResultMetadata(const QString& providerId,
                                                       const ProviderFetchResult& result)
{
    m_lastFetchAttempts[providerId] = result.attempts;
    emit fetchAttemptsChanged(providerId);

    if (result.dashboard.has_value()) {
        m_dashboardData[providerId] = result.dashboard->toVariantMap();
    } else {
        m_dashboardData.remove(providerId);
    }
}

void ProviderRefreshCoordinator::clearResultMetadata(const QString& providerId)
{
    m_lastFetchAttempts[providerId] = {};
    m_dashboardData.remove(providerId);
    emit fetchAttemptsChanged(providerId);
}
