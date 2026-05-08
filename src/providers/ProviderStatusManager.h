#pragma once

#include "ProviderCatalogSnapshot.h"
#include "shared/ProviderStatusFetcher.h"

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QVariantMap>

class UsageBackend;
class SettingsStore;

class ProviderStatusManager : public QObject {
    Q_OBJECT
public:
    explicit ProviderStatusManager(QObject* parent = nullptr);

    // Status access
    QVariantMap status(const QString& providerId) const;
    QString statusURL(const QString& providerId, const ProviderCatalogEntry& entry) const;
    int revision() const { return m_revision; }

    // Status polling
    void configurePolling(bool enabled, int intervalMs = 5 * 60 * 1000);
    void refreshStatuses(const ProviderCatalogSnapshot& catalog, UsageBackend* backend);

    // Status updates (called from backend result handler)
    void setStatus(const QString& providerId, const QVariantMap& status);
    void setStatuses(const QHash<QString, QVariantMap>& statuses);

    // Backend result handling
    void handleBackendResult(int generation, bool success, const QVariant& payload);

    // Settings dependency
    void setSettingsStore(SettingsStore* store) { m_settingsStore = store; }

signals:
    void statusChanged(const QString& providerId);
    void revisionChanged();

private slots:
    void onPollTimer();

private:
    QTimer m_pollTimer;
    QHash<QString, QVariantMap> m_statuses;
    int m_revision = 0;
    int m_refreshGeneration = 0;
    SettingsStore* m_settingsStore = nullptr;
};
