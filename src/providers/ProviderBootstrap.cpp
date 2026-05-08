#include "ProviderBootstrap.h"

#include "ProviderCatalogSnapshot.h"
#include "ProviderRegistry.h"

// Provider includes — alphabetically ordered, compile-time checked
#include "abacus/AbacusProvider.h"
#include "alibaba/AlibabaProvider.h"
#include "amp/AmpProvider.h"
#include "antigravity/AntigravityProvider.h"
#include "augment/AugmentProvider.h"
#include "claude/ClaudeProvider.h"
#include "codebuff/CodebuffProvider.h"
#include "codex/CodexProvider.h"
#include "copilot/CopilotProvider.h"
#include "cursor/CursorProvider.h"
#include "deepseek/DeepSeekProvider.h"
#include "factory/FactoryProvider.h"
#include "gemini/GeminiProvider.h"
#include "jetbrains/JetBrainsProvider.h"
#include "kilo/KiloProvider.h"
#include "kimi/KimiProvider.h"
#include "kimik2/KimiK2Provider.h"
#include "kiro/KiroProvider.h"
#include "minimax/MiniMaxProvider.h"
#include "mistral/MistralProvider.h"
#include "ollama/OllamaProvider.h"
#include "opencode/OpenCodeProvider.h"
#include "opencode/OpenCodeGoProvider.h"
#include "openrouter/OpenRouterProvider.h"
#include "perplexity/PerplexityProvider.h"
#include "synthetic/SyntheticProvider.h"
#include "vertexai/VertexAIProvider.h"
#include "warp/WarpProvider.h"
#include "windsurf/WindsurfProvider.h"
#include "zai/ZaiProvider.h"

#include "../app/SettingsStore.h"
#include "../app/UsageStore.h"
#include "../runtime/ProviderRuntimeManager.h"

#include <QSettings>
#include <QString>

namespace {

template <typename Provider>
void registerProviderIfMissing(const QString& id)
{
    auto& registry = ProviderRegistry::instance();
    if (!registry.provider(id)) {
        registry.registerProvider(new Provider());
    }
}

} // namespace

namespace ProviderBootstrap {

void registerAllProviders()
{
#define CODEXBAR_PROVIDER(ClassName, stringId, enumName) \
    registerProviderIfMissing<ClassName##Provider>(QStringLiteral(stringId));
#include "ProviderDefs.def"
}

void applyStoredProviderEnabledStates(SettingsStore* settings, UsageStore* usageStore)
{
    Q_UNUSED(settings)
    auto& registry = ProviderRegistry::instance();
    const ProviderCatalogSnapshot catalog = ProviderCatalogSnapshot::fromRegistry(registry, 0);
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\CodexBar"), QSettings::NativeFormat);
    for (const auto& provider : catalog.providers()) {
        const QString& id = provider.id;
        const QString key = QStringLiteral("providers/") + id + QStringLiteral("/enabled");
        bool enabled = false;
        if (reg.contains(key)) {
            enabled = reg.value(key).toBool();
        } else {
            enabled = provider.defaultEnabled;
        }

        if (usageStore) {
            usageStore->setProviderEnabled(id, enabled);
        } else {
            registry.setProviderEnabled(id, enabled);
        }
    }
}

void syncEnabledProviderRuntimes()
{
    ProviderRuntimeManager::instance()->syncEnabledProviderRuntimes();
}

} // namespace ProviderBootstrap

std::optional<UsageProvider> usageProviderFromString(const QString& id)
{
#define CODEXBAR_PROVIDER(ClassName, stringId, enumName) \
    if (id == QLatin1String(stringId)) return UsageProvider::enumName;
#include "ProviderDefs.def"
    return std::nullopt;
}
