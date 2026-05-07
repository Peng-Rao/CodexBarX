#pragma once

#include "IFetchStrategy.h"
#include "../models/ProviderDescriptor.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <optional>

class ProviderRegistry;

struct ProviderCatalogEntry {
    QString id;
    bool enabled = false;
    bool defaultEnabled = false;
    ProviderDescriptor descriptor;
    bool hasDescriptor = false;
    QString brandColor;
    QVector<ProviderSettingsDescriptor> settingsDescriptors;
};

class ProviderCatalogSnapshot {
public:
    static ProviderCatalogSnapshot fromRegistry(const ProviderRegistry& registry, int generation);

    int generation() const { return m_generation; }
    const QVector<ProviderCatalogEntry>& providers() const { return m_providers; }
    std::optional<ProviderCatalogEntry> provider(const QString& id) const;
    QVector<QString> providerIDs() const;
    QVector<QString> enabledProviderIDs() const;

private:
    int m_generation = 0;
    QVector<ProviderCatalogEntry> m_providers;
    QHash<QString, ProviderCatalogEntry> m_byId;
};
