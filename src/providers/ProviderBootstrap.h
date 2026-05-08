#pragma once

#include <QString>
#include <optional>

enum class UsageProvider : int;

class SettingsStore;
class UsageStore;

namespace ProviderBootstrap {

void registerAllProviders();
void applyStoredProviderEnabledStates(SettingsStore* settings, UsageStore* usageStore);
void syncEnabledProviderRuntimes();

} // namespace ProviderBootstrap

std::optional<UsageProvider> usageProviderFromString(const QString& id);
