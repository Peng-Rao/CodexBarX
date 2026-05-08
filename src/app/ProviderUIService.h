#pragma once

#include <QObject>
#include <QHash>
#include <QVariantMap>
#include <QVariantList>
#include <QString>
#include <QSet>
#include <functional>

class ProviderCatalogSnapshot;
class ProviderStatusManager;
class ProviderCredentialManager;
class SettingsStore;
class UsageBackend;

/**
 * @brief UI data building and caching for provider lists and descriptors.
 *
 * Handles providerList(), providerDescriptorData() and their async variants.
 * Caches results to avoid redundant computation on every UI frame.
 *
 * Extracted from UsageStore during Phase 5 refactoring.
 */
class ProviderUIService : public QObject {
    Q_OBJECT
public:
    explicit ProviderUIService(QObject* parent = nullptr);

    // === Provider List ===
    QVariantList providerList() const;
    void requestProviderList();

    // === Provider Descriptor ===
    QVariantMap providerDescriptorData(const QString& id) const;
    void requestProviderDescriptor(const QString& providerId);

    // === Cache Invalidation ===
    void invalidateProviderListCache();
    void invalidateDescriptorCache(const QString& providerId);
    void invalidateAllCaches();

    // === Cache State ===
    bool isProviderListCacheValid() const { return m_providerListCacheValid; }
    QVariantList cachedProviderList() const { return m_providerListCache; }

    // === Dependencies ===
    void setCatalog(const ProviderCatalogSnapshot* catalog);
    void setStatusManager(ProviderStatusManager* manager);
    void setCredentialManager(ProviderCredentialManager* manager);
    void setSettingsStore(SettingsStore* store);
    void setBackend(UsageBackend* backend);

    // === Callbacks for data access ===
    // These allow the service to get data from UsageStore without tight coupling
    using SnapshotAccessor = std::function<std::optional<double>(const QString& providerId)>;
    using ErrorAccessor = std::function<QString(const QString& providerId)>;
    using SecretStatusAccessor = std::function<QVariantMap(const QString& providerId, const QString& key)>;
    using DisplayNameAccessor = std::function<QString(const QString& providerId)>;
    using StatusURLAccessor = std::function<QString(const QString& providerId)>;

    void setSnapshotAccessor(SnapshotAccessor accessor);
    void setErrorAccessor(ErrorAccessor accessor);
    void setSecretStatusAccessor(SecretStatusAccessor accessor);
    void setDisplayNameAccessor(DisplayNameAccessor accessor);
    void setStatusURLAccessor(StatusURLAccessor accessor);

    // === Generation tracking ===
    int listGeneration() const { return m_listGeneration; }

    // === Backend result handling ===
    bool handleProviderListResult(int generation, const QVariantList& providers);
    bool handleDescriptorResult(const QString& providerId, int generation, const QVariantMap& descriptor);

signals:
    void providerListModelChanged();
    void providerDescriptorChanged(const QString& providerId);

private:
    QVariantList buildProviderListNow() const;
    QVariantMap buildDescriptorDataNow(const QString& id) const;

    // Dependencies (non-owning)
    const ProviderCatalogSnapshot* m_catalog = nullptr;
    ProviderStatusManager* m_statusManager = nullptr;
    ProviderCredentialManager* m_credentialManager = nullptr;
    SettingsStore* m_settingsStore = nullptr;
    UsageBackend* m_backend = nullptr;

    // Accessors
    SnapshotAccessor m_snapshotAccessor;
    ErrorAccessor m_errorAccessor;
    SecretStatusAccessor m_secretStatusAccessor;
    DisplayNameAccessor m_displayNameAccessor;
    StatusURLAccessor m_statusURLAccessor;

    // Provider list cache
    mutable QVariantList m_providerListCache;
    mutable bool m_providerListCacheValid = false;
    mutable bool m_providerListRefreshQueued = false;
    int m_listGeneration = 0;

    // Provider descriptor cache
    mutable QHash<QString, QVariantMap> m_descriptorCache;
    mutable QSet<QString> m_descriptorRefreshQueued;
    QHash<QString, int> m_descriptorGenerations;
};
