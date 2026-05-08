#include "ProviderStatusManager.h"
#include "../app/UsageBackend.h"
#include "../app/UsageBackendTypes.h"
#include "../app/SettingsStore.h"
#include "../network/NetworkManager.h"

#include <QJsonObject>

ProviderStatusManager::ProviderStatusManager(QObject* parent)
    : QObject(parent)
{
    connect(&m_pollTimer, &QTimer::timeout, this, &ProviderStatusManager::onPollTimer);
}

QVariantMap ProviderStatusManager::status(const QString& providerId) const
{
    return m_statuses.value(providerId, QVariantMap{{QStringLiteral("state"), QStringLiteral("unknown")}});
}

QString ProviderStatusManager::statusURL(const QString& providerId, const ProviderCatalogEntry& entry) const
{
    if (!entry.hasDescriptor) return {};
    const auto& desc = entry.descriptor;
    return ProviderStatusFetcher::openURL(
        desc.metadata.statusPageURL,
        desc.metadata.statusLinkURL,
        desc.metadata.statusWorkspaceProductID);
}

void ProviderStatusManager::configurePolling(bool enabled, int intervalMs)
{
    if (!enabled) {
        m_pollTimer.stop();
        return;
    }
    m_pollTimer.start(intervalMs);
}

void ProviderStatusManager::refreshStatuses(const ProviderCatalogSnapshot& catalog, UsageBackend* backend)
{
    if (!backend) return;

    const bool enabled = m_settingsStore ? m_settingsStore->statusChecksEnabled() : true;
    if (!enabled) return;

    QVector<ProviderStatusPollTarget> targets;
    for (const auto& provider : catalog.providers()) {
        if (!provider.hasDescriptor) continue;
        const auto& desc = provider.descriptor;

        const auto providerTargets = ProviderStatusFetcher::buildPollTargets(
            {provider.id}, desc.metadata.statusPageURL, desc.metadata.statusLinkURL,
            desc.metadata.statusWorkspaceProductID);
        targets.append(providerTargets);
    }

    const int generation = ++m_refreshGeneration;
    backend->dispatchValueJob(QStringLiteral("providerStatuses"), generation,
        [targets]() -> QVariant {
            QHash<QString, QVariantMap> statuses;
            QVector<ProviderStatusPollTarget> statuspageTargets;
            QVector<ProviderStatusPollTarget> workspaceTargets;

            for (const auto& target : targets) {
                if (target.source == ProviderStatusSource::Statuspage) {
                    statuspageTargets.append(target);
                } else {
                    workspaceTargets.append(target);
                }
            }

            for (const auto& target : statuspageTargets) {
                if (target.requestURL.isEmpty()) {
                    statuses[target.providerId] = {{QStringLiteral("state"), QStringLiteral("unknown")}};
                    continue;
                }

                QJsonObject json = NetworkManager::instance().getJsonSync(target.requestURL, {}, 8000);
                ProviderStatusSnapshot snap;
                if (!json.isEmpty()) {
                    snap = ProviderStatusFetcher::parseStatuspageResponse(json, target.statusPageURL);
                } else {
                    snap.state = QStringLiteral("unknown");
                    snap.source = QStringLiteral("statuspage");
                    snap.statusURL = target.statusPageURL;
                    snap.updatedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();
                }
                statuses[target.providerId] = snap.toVariantMap();
            }

            if (!workspaceTargets.isEmpty()) {
                const QUrl workspaceURL(QStringLiteral(
                    "https://www.google.com/appsstatus/dashboard/incidents.json"));
                const QString response = NetworkManager::instance().getStringSync(workspaceURL, {}, 8000);
                const QByteArray rawData = response.toUtf8();

                if (rawData.trimmed().isEmpty()) {
                    for (const auto& target : workspaceTargets) {
                        if (statuses.contains(target.providerId)) continue;
                        QVariantMap map;
                        map[QStringLiteral("state")] = QStringLiteral("unknown");
                        map[QStringLiteral("source")] = QStringLiteral("workspace");
                        map[QStringLiteral("statusURL")] = target.statusLinkURL;
                        map[QStringLiteral("updatedAt")] = QDateTime::currentDateTime().toMSecsSinceEpoch();
                        statuses[target.providerId] = map;
                    }
                } else {
                    for (const auto& target : workspaceTargets) {
                        if (statuses.contains(target.providerId)) continue;
                        auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
                            rawData, target.workspaceProductID, target.statusLinkURL);
                        statuses[target.providerId] = snap.toVariantMap();
                    }
                }
            }

            ProviderStatusesPayload payload;
            payload.statuses = statuses;
            return QVariant::fromValue(payload);
        });
}

void ProviderStatusManager::setStatus(const QString& providerId, const QVariantMap& status)
{
    m_statuses[providerId] = status;
    m_revision++;
    emit revisionChanged();
    emit statusChanged(providerId);
}

void ProviderStatusManager::setStatuses(const QHash<QString, QVariantMap>& statuses)
{
    if (statuses.isEmpty()) return;

    for (auto it = statuses.constBegin(); it != statuses.constEnd(); ++it) {
        m_statuses[it.key()] = it.value();
    }

    m_revision++;
    emit revisionChanged();
    for (auto it = statuses.constBegin(); it != statuses.constEnd(); ++it) {
        emit statusChanged(it.key());
    }
}

void ProviderStatusManager::handleBackendResult(int generation, bool success, const QVariant& payload)
{
    if (generation != m_refreshGeneration) {
        return;
    }
    if (!success) {
        qWarning() << "Provider status backend job failed";
        return;
    }

    const auto p = payload.value<ProviderStatusesPayload>();
    setStatuses(p.statuses);
}

void ProviderStatusManager::onPollTimer()
{
    // Caller should connect this to refreshStatuses with proper catalog/backend
    emit revisionChanged(); // Signal that a poll is due
}
