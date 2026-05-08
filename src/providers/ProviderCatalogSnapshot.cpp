#include "ProviderCatalogSnapshot.h"

#include "IProvider.h"
#include "ProviderRegistry.h"

namespace {

ProviderDescriptor descriptorFromProvider(IProvider* provider)
{
    ProviderDescriptor descriptor;
    if (!provider) {
        return descriptor;
    }

    descriptor.id = provider->id();
    descriptor.metadata.displayName = provider->displayName();
    descriptor.metadata.sessionLabel = provider->sessionLabel();
    descriptor.metadata.weeklyLabel = provider->weeklyLabel();
    const QString opus = provider->opusLabel();
    if (!opus.isEmpty()) {
        descriptor.metadata.opusLabel = opus;
    }
    descriptor.metadata.supportsCredits = provider->supportsCredits();
    descriptor.metadata.cliName = provider->cliName();
    descriptor.metadata.statusPageURL = provider->statusPageURL();
    descriptor.metadata.statusLinkURL = provider->statusLinkURL();
    descriptor.metadata.statusWorkspaceProductID = provider->statusWorkspaceProductID();
    descriptor.metadata.dashboardURL = provider->dashboardURL();
    descriptor.metadata.subscriptionDashboardURL = provider->subscriptionDashboardURL();
    descriptor.fetchPlan.allowedSourceModes = provider->supportedSourceModes();
    descriptor.fetchPlan.defaultSourceMode = descriptor.fetchPlan.allowedSourceModes.contains(QStringLiteral("auto"))
        ? QStringLiteral("auto")
        : (descriptor.fetchPlan.allowedSourceModes.isEmpty()
               ? QStringLiteral("auto")
               : descriptor.fetchPlan.allowedSourceModes.first());
    descriptor.cli.name = provider->cliName();
    descriptor.tokenAccounts.supportsMultipleAccounts = provider->supportsMultipleAccounts();
    descriptor.tokenAccounts.requiredCredentialTypes = provider->requiredCredentialTypes();
    return descriptor;
}

} // namespace

ProviderCatalogSnapshot ProviderCatalogSnapshot::fromRegistry(const ProviderRegistry& registry, int generation)
{
    ProviderCatalogSnapshot snapshot;
    snapshot.m_generation = generation;

    const QVector<QString> ids = registry.providerIDs();
    snapshot.m_providers.reserve(ids.size());
    for (const QString& id : ids) {
        IProvider* provider = registry.provider(id);
        ProviderCatalogEntry entry;
        entry.id = id;
        entry.enabled = registry.isProviderEnabled(id);
        entry.defaultEnabled = provider ? provider->defaultEnabled() : false;
        if (auto descriptor = registry.descriptor(id); descriptor.has_value()) {
            entry.descriptor = descriptor.value();
            entry.hasDescriptor = true;
        } else if (provider) {
            entry.descriptor = descriptorFromProvider(provider);
            entry.hasDescriptor = true;
        }
        entry.brandColor = provider ? provider->brandColor() : entry.descriptor.branding.color;
        if (entry.brandColor.isEmpty()) {
            entry.brandColor = QStringLiteral("#6b6bff");
        }
        entry.settingsDescriptors = provider ? provider->settingsDescriptors() : QVector<ProviderSettingsDescriptor>{};

        snapshot.m_byId.insert(id, entry);
        snapshot.m_providers.append(entry);
    }

    return snapshot;
}

std::optional<ProviderCatalogEntry> ProviderCatalogSnapshot::provider(const QString& id) const
{
    auto it = m_byId.constFind(id);
    if (it == m_byId.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

QVector<QString> ProviderCatalogSnapshot::providerIDs() const
{
    QVector<QString> ids;
    ids.reserve(m_providers.size());
    for (const auto& provider : m_providers) {
        ids.append(provider.id);
    }
    return ids;
}

QVector<QString> ProviderCatalogSnapshot::enabledProviderIDs() const
{
    QVector<QString> ids;
    for (const auto& provider : m_providers) {
        if (provider.enabled) {
            ids.append(provider.id);
        }
    }
    return ids;
}
