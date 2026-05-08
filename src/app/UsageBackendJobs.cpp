#include "UsageBackendJobs.h"

#include "../account/TokenAccountStore.h"
#include "../providers/IProvider.h"
#include "../providers/ProviderPipeline.h"
#include "../providers/ProviderRegistry.h"
#include "../providers/ProviderSourceMode.h"
#include "../providers/shared/ProviderCredentialStore.h"
#include "../runtime/IProviderRuntime.h"
#include "../runtime/ProviderRuntimeManager.h"

#include <QDateTime>
#include <QSet>

namespace UsageBackendJobs {
namespace {

QVariant providerSetting(const ProviderFetchCommandInput& input,
                         const QString& key,
                         const QVariant& defaultValue = QVariant())
{
    const auto it = input.providerSettings.constFind(key);
    return it == input.providerSettings.constEnd() ? defaultValue : it.value();
}

bool isSourceModeAllowed(const QString& providerId, ProviderSourceMode mode)
{
    if (mode == ProviderSourceMode::Auto) return true;
    auto desc = ProviderRegistry::instance().descriptor(providerId);
    if (!desc.has_value()) return true;
    return desc->fetchPlan.allowedSourceModes.contains(sourceModeToString(mode));
}

struct FetchContextBuildResult {
    ProviderFetchContext context;
    QVector<CredentialCacheUpdatePayload> credentialUpdates;
};

FetchContextBuildResult buildFetchContext(const ProviderFetchCommandInput& input)
{
    FetchContextBuildResult output;
    ProviderFetchContext& ctx = output.context;
    const QString& providerId = input.providerId;
    ctx.providerId = providerId;
    ctx.sourceMode = ProviderSourceMode::Auto;
    ctx.isAppRuntime = true;
    ctx.allowInteractiveAuth = false;
    ctx.networkTimeoutMs = ProviderPipeline::STRATEGY_TIMEOUT_MS;
    ctx.env = input.env;

    auto addSetting = [&](const QString& key, const QVariant& defaultValue = QVariant()) {
        QVariant value = providerSetting(input, key, defaultValue);
        if (value.isValid()) {
            ctx.settings.set(key, value);
        }
        return value;
    };

    for (const auto& field : input.settingsFields) {
        const auto& descriptor = field.descriptor;
        if (descriptor.sensitive) {
            QString secret;
            if (!descriptor.envVar.isEmpty() && ctx.env.contains(descriptor.envVar)) {
                secret = ctx.env.value(descriptor.envVar).trimmed();
            }
            if (secret.isEmpty() && !descriptor.credentialTarget.isEmpty()) {
                const auto cacheIt = input.credentialCache.constFind(descriptor.credentialTarget);
                const bool knownMissing = cacheIt != input.credentialCache.constEnd() && cacheIt->missing;
                if (cacheIt != input.credentialCache.constEnd() && cacheIt->hasValue) {
                    secret = QString::fromUtf8(cacheIt->value).trimmed();
                } else if (!knownMissing) {
                    const auto stored = ProviderCredentialStore::read(descriptor.credentialTarget);
                    CredentialCacheUpdatePayload update;
                    update.target = descriptor.credentialTarget;
                    update.exists = stored.has_value();
                    if (stored.has_value()) {
                        update.data = stored.value();
                        secret = QString::fromUtf8(update.data).trimmed();
                    }
                    output.credentialUpdates.append(update);
                }
            }
            if (secret.isEmpty()) {
                secret = addSetting(descriptor.key, descriptor.defaultValue).toString().trimmed();
            } else {
                ctx.settings.set(descriptor.key, secret);
            }
        } else {
            addSetting(descriptor.key, descriptor.defaultValue);
        }
    }

    QString sourceMode = addSetting(QStringLiteral("sourceMode"), QStringLiteral("auto")).toString();
    if (sourceMode == QLatin1String("auto")) {
        if (providerId == QLatin1String("codex")) {
            sourceMode = addSetting(QStringLiteral("codexDataSource"), QStringLiteral("auto")).toString();
        } else if (providerId == QLatin1String("claude")) {
            sourceMode = addSetting(QStringLiteral("claudeDataSource"), QStringLiteral("auto")).toString();
        }
    }
    ctx.settings.set(QStringLiteral("sourceMode"), sourceMode);
    ctx.sourceMode = sourceModeFromString(sourceMode);

    QString cookieSource = addSetting(
        QStringLiteral("cookieSource"),
        ctx.settings.get(QStringLiteral("cookieSource"), QStringLiteral("auto"))).toString();
    if (providerId == QLatin1String("cursor") && cookieSource == QLatin1String("auto")) {
        cookieSource = addSetting(QStringLiteral("cursorCookieSource"), QStringLiteral("auto")).toString();
    }
    ctx.settings.set(QStringLiteral("cookieSource"), cookieSource);

    QString manualCookie = ctx.settings.get(QStringLiteral("manualCookieHeader")).toString().trimmed();
    if (manualCookie.isEmpty()) {
        manualCookie = addSetting(QStringLiteral("manualCookieHeader"), QString()).toString().trimmed();
    }
    if (!manualCookie.isEmpty()) {
        ctx.manualCookieHeader = manualCookie;
    }

    QString accountId = addSetting(QStringLiteral("accountID"), QString()).toString().trimmed();
    ctx.accountID = accountId;

    if (providerId == QLatin1String("codex")) {
        if (!input.codexActiveAccountId.isEmpty()) {
            ctx.accountID = input.codexActiveAccountId;
        }
        if (!input.codexManagedHomePath.isEmpty()) {
            ctx.env[QStringLiteral("CODEX_HOME")] = input.codexManagedHomePath;
        }
    }

    bool ok = false;
    int timeout = addSetting(QStringLiteral("networkTimeoutMs"), ProviderPipeline::STRATEGY_TIMEOUT_MS).toInt(&ok);
    if (ok && timeout > 0) {
        ctx.networkTimeoutMs = timeout;
    }

    QString apiRegion = addSetting(QStringLiteral("apiRegion"), QStringLiteral("global")).toString();
    if (providerId == QLatin1String("zai")) {
        ctx.env[QStringLiteral("ZAI_API_REGION")] = apiRegion;
    }

    QString resolvedAccountId = ctx.accountID;
    if (resolvedAccountId.isEmpty()) {
        resolvedAccountId = input.defaultTokenAccountId;
    }
    if (!resolvedAccountId.isEmpty()) {
        auto accOpt = TokenAccountStore::instance()->accountWithCredentials(resolvedAccountId);
        if (accOpt.has_value()) {
            const TokenAccount& acc = accOpt.value();
            ctx.accountID = acc.accountId;
            ctx.accountCredentials = acc.credentials;
            if (acc.sourceMode != ProviderSourceMode::Auto) {
                ctx.sourceMode = acc.sourceMode;
            }
        }
    }

    return output;
}

ProviderFetchResult fetchProvider(IProvider* provider, const ProviderFetchContext& ctx, bool useRuntime)
{
    ProviderFetchResult result;
    if (!provider) {
        result.success = false;
        result.errorMessage = QStringLiteral("Unknown provider");
        return result;
    }

    if (!isSourceModeAllowed(ctx.providerId, ctx.sourceMode)) {
        result.success = false;
        result.errorMessage = QStringLiteral("unsupported source mode: %1")
            .arg(sourceModeToString(ctx.sourceMode));
        return result;
    }

    if (useRuntime) {
        if (ProviderRuntimeManager* rtMgr = ProviderRuntimeManager::instance()) {
            if (IProviderRuntime* runtime = rtMgr->runtimeFor(ctx.providerId)) {
                if (runtime->state() == RuntimeState::Running) {
                    return runtime->fetch(ctx);
                }
            }
        }
    }

    ProviderPipeline pipeline;
    return pipeline.executeProvider(provider, ctx);
}

} // namespace

ProviderRefreshPayload refreshProvider(IProvider* provider,
                                       const ProviderFetchCommandInput& input)
{
    const FetchContextBuildResult build = buildFetchContext(input);

    ProviderRefreshPayload payload;
    payload.providerId = input.providerId;
    payload.fetchResult = fetchProvider(provider, build.context, true);
    payload.credentialUpdates = build.credentialUpdates;
    return payload;
}

ProviderConnectionTestPayload testProviderConnection(IProvider* provider,
                                                     const ProviderFetchCommandInput& input,
                                                     qint64 startedAt)
{
    FetchContextBuildResult build = buildFetchContext(input);
    build.context.allowInteractiveAuth = false;

    ProviderConnectionTestPayload payload;
    payload.providerId = input.providerId;
    payload.fetchResult = fetchProvider(provider, build.context, false);
    if (!payload.fetchResult.success
        && payload.fetchResult.errorMessage.startsWith(QLatin1String("unsupported source mode:"))) {
        payload.fetchResult.errorMessage.replace(0, 1, QStringLiteral("U"));
    }
    payload.startedAt = startedAt;
    payload.credentialUpdates = build.credentialUpdates;
    return payload;
}

CredentialPreloadPayload preloadCredentials(const QVector<CredentialPreloadItem>& items)
{
    CredentialPreloadPayload payload;
    QSet<QString> seenTargets;
    for (const auto& item : items) {
        if (item.target.isEmpty() || seenTargets.contains(item.target)) {
            continue;
        }
        seenTargets.insert(item.target);

        const auto stored = ProviderCredentialStore::read(item.target);
        CredentialCacheUpdatePayload update;
        update.target = item.target;
        update.exists = stored.has_value();
        if (stored.has_value()) {
            update.data = stored.value();
        }
        payload.updates.append(update);
    }
    return payload;
}

} // namespace UsageBackendJobs
