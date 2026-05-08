#include "CostUsageService.h"

#include "../util/CostUsageCache.h"
#include "../util/CostUsageScanner.h"

#include <QDate>
#include <QHash>

#include <algorithm>

namespace {

CostUsageSnapshot mergeCostUsageSnapshots(const QVector<CostUsageSnapshot>& snapshots)
{
    CostUsageSnapshot merged;
    merged.updatedAt = QDateTime::currentDateTime();

    QHash<QString, CostUsageDailyEntry> dayMap;
    for (const auto& snap : snapshots) {
        merged.sessionTokens += snap.sessionTokens;
        merged.sessionCostUSD += snap.sessionCostUSD;
        merged.last30DaysTokens += snap.last30DaysTokens;
        merged.last30DaysCostUSD += snap.last30DaysCostUSD;

        for (const auto& day : snap.daily) {
            auto& entry = dayMap[day.date];
            entry.date = day.date;
            entry.inputTokens += day.inputTokens;
            entry.cacheReadTokens += day.cacheReadTokens;
            entry.cacheCreationTokens += day.cacheCreationTokens;
            entry.outputTokens += day.outputTokens;
            entry.costUSD += day.costUSD;
            for (const auto& model : day.models) {
                entry.models.append(model);
            }
        }
    }

    for (auto it = dayMap.constBegin(); it != dayMap.constEnd(); ++it) {
        merged.daily.append(it.value());
    }
    std::sort(merged.daily.begin(), merged.daily.end(),
              [](const CostUsageDailyEntry& a, const CostUsageDailyEntry& b) {
                  return a.date < b.date;
              });

    return merged;
}

void mergeProviderSnapshot(CostUsageSnapshot& existing, const CostUsageSnapshot& snapshot)
{
    existing.sessionTokens += snapshot.sessionTokens;
    existing.sessionCostUSD += snapshot.sessionCostUSD;
    existing.last30DaysTokens += snapshot.last30DaysTokens;
    existing.last30DaysCostUSD += snapshot.last30DaysCostUSD;

    QHash<QString, CostUsageDailyEntry> dayMap;
    for (auto& day : existing.daily) {
        dayMap[day.date] = day;
    }
    for (auto& day : snapshot.daily) {
        auto& entry = dayMap[day.date];
        entry.date = day.date;
        entry.inputTokens += day.inputTokens;
        entry.cacheReadTokens += day.cacheReadTokens;
        entry.cacheCreationTokens += day.cacheCreationTokens;
        entry.outputTokens += day.outputTokens;
        entry.costUSD += day.costUSD;
        for (auto& model : day.models) {
            entry.models.append(model);
        }
    }
    existing.daily = dayMap.values();
    std::sort(existing.daily.begin(), existing.daily.end(),
              [](const CostUsageDailyEntry& a, const CostUsageDailyEntry& b) {
                  return a.date < b.date;
              });
}

QVector<ProviderCostUsageSnapshot> buildProviderSnapshots(
    const QHash<QString, CostUsageSnapshot>& perProvider)
{
    QVector<ProviderCostUsageSnapshot> allProviders;
    for (auto it = perProvider.constBegin(); it != perProvider.constEnd(); ++it) {
        ProviderCostUsageSnapshot snapshot;
        snapshot.providerId = it.key();
        snapshot.snapshot = it.value();

        QHash<QString, CostUsageModelBreakdown> modelMap;
        for (auto& entry : it.value().daily) {
            for (auto& model : entry.models) {
                auto& aggregate = modelMap[model.modelName];
                aggregate.modelName = model.modelName;
                aggregate.inputTokens += model.inputTokens;
                aggregate.cacheReadTokens += model.cacheReadTokens;
                aggregate.cacheCreationTokens += model.cacheCreationTokens;
                aggregate.outputTokens += model.outputTokens;
                aggregate.costUSD += model.costUSD;
            }
        }

        snapshot.modelSummary = modelMap.values();
        std::sort(snapshot.modelSummary.begin(), snapshot.modelSummary.end(),
                  [](const CostUsageModelBreakdown& a, const CostUsageModelBreakdown& b) {
                      return a.costUSD > b.costUSD;
                  });
        allProviders.append(snapshot);
    }

    std::sort(allProviders.begin(), allProviders.end(),
              [](const ProviderCostUsageSnapshot& a, const ProviderCostUsageSnapshot& b) {
                  return a.snapshot.last30DaysCostUSD > b.snapshot.last30DaysCostUSD;
              });
    return allProviders;
}

bool scansLocalUsageWithoutTokenAccount(const QString& providerId)
{
    return providerId == QLatin1String("claude")
        || providerId == QLatin1String("codex")
        || providerId == QLatin1String("opencode")
        || providerId == QLatin1String("opencodego");
}

QString usageDetailsFallbackBrandColor(const QString& providerId)
{
    static const QHash<QString, QString> colors = {
        {QStringLiteral("codex"), QStringLiteral("#49A3B0")},
        {QStringLiteral("claude"), QStringLiteral("#CC7C5E")},
        {QStringLiteral("cursor"), QStringLiteral("#5B8DFA")},
        {QStringLiteral("gemini"), QStringLiteral("#8860D0")},
        {QStringLiteral("copilot"), QStringLiteral("#2DA44E")},
        {QStringLiteral("zai"), QStringLiteral("#E85A6A")},
        {QStringLiteral("opencode"), QStringLiteral("#E44D26")},
        {QStringLiteral("opencodego"), QStringLiteral("#3B82F6")},
        {QStringLiteral("warp"), QStringLiteral("#00BCD4")},
        {QStringLiteral("mistral"), QStringLiteral("#F77F00")},
        {QStringLiteral("openrouter"), QStringLiteral("#FF6B6B")},
        {QStringLiteral("ollama"), QStringLiteral("#E6EF6C")},
        {QStringLiteral("kilo"), QStringLiteral("#7C3AED")},
        {QStringLiteral("kiro"), QStringLiteral("#F59E0B")},
        {QStringLiteral("kimik2"), QStringLiteral("#06B6D4")},
        {QStringLiteral("minimax"), QStringLiteral("#EC4899")},
        {QStringLiteral("perplexity"), QStringLiteral("#22C55E")},
        {QStringLiteral("kimi"), QStringLiteral("#8B5CF6")},
        {QStringLiteral("abacus"), QStringLiteral("#6366F1")},
        {QStringLiteral("alibaba"), QStringLiteral("#F97316")},
        {QStringLiteral("augment"), QStringLiteral("#14B8A6")},
        {QStringLiteral("amp"), QStringLiteral("#D946EF")},
        {QStringLiteral("factory"), QStringLiteral("#84CC16")},
        {QStringLiteral("jetbrains"), QStringLiteral("#F000F0")},
        {QStringLiteral("vertexai"), QStringLiteral("#4285F4")},
        {QStringLiteral("deepseek"), QStringLiteral("#4D6BFE")},
        {QStringLiteral("antigravity"), QStringLiteral("#10B981")},
        {QStringLiteral("synthetic"), QStringLiteral("#6366F1")},
    };
    return colors.value(providerId, QStringLiteral("#4A90D9"));
}

QString usageDetailsDisplayNameFor(const QString& providerId, const QVariantMap& provider)
{
    const QString providerName = provider.value(QStringLiteral("name")).toString();
    if (!providerName.isEmpty()) {
        return providerName;
    }
    static const QHash<QString, QString> names = {
        {QStringLiteral("codex"), QStringLiteral("Codex")},
        {QStringLiteral("claude"), QStringLiteral("Claude")},
        {QStringLiteral("opencodego"), QStringLiteral("OpenCode Go")},
        {QStringLiteral("opencode"), QStringLiteral("OpenCode")},
        {QStringLiteral("kimi"), QStringLiteral("Kimi")},
        {QStringLiteral("kimik2"), QStringLiteral("Kimi K2")},
        {QStringLiteral("copilot"), QStringLiteral("Copilot")},
        {QStringLiteral("cursor"), QStringLiteral("Cursor")},
    };
    return names.value(providerId, providerId);
}

QString usageDetailsBrandColorFor(const QString& providerId, const QVariantMap& provider)
{
    const QString providerColor = provider.value(QStringLiteral("brandColor")).toString();
    return providerColor.isEmpty() ? usageDetailsFallbackBrandColor(providerId) : providerColor;
}

QString usageDetailsProviderKind(const QVariantMap& provider)
{
    const QString id = provider.value(QStringLiteral("id")).toString();
    if (id.isEmpty()) {
        return {};
    }
    if (id == QLatin1String("codex") || id == QLatin1String("claude") ||
        id == QLatin1String("opencodego") || id == QLatin1String("opencode")) {
        return QStringLiteral("token");
    }
    if (id == QLatin1String("kimi") || id == QLatin1String("kimik2")) {
        return QStringLiteral("credit");
    }
    if (id == QLatin1String("copilot") || id == QLatin1String("cursor")) {
        return QStringLiteral("quota");
    }
    if (provider.value(QStringLiteral("supportsCredits"), false).toBool()) {
        return QStringLiteral("credit");
    }
    return {};
}

QVariantList usageDetailsDailyEntries(const CostUsageSnapshot& snapshot)
{
    QVariantList daily;
    for (const auto& d : snapshot.daily) {
        if (d.totalTokens() == 0) continue;
        QVariantMap dm;
        dm[QStringLiteral("date")] = d.date;
        dm[QStringLiteral("totalTokens")] = d.totalTokens();
        dm[QStringLiteral("costUSD")] = d.costUSD;
        daily.append(dm);
    }
    return daily;
}

QVariantList usageDetailsModelEntries(const ProviderCostUsageSnapshot& provider)
{
    QVariantList models;
    for (const auto& model : provider.modelSummary) {
        QVariantMap mm;
        mm[QStringLiteral("name")] = model.modelName;
        mm[QStringLiteral("tokens")] = model.totalTokens();
        mm[QStringLiteral("costUSD")] = model.costUSD;
        models.append(mm);
    }
    return models;
}

QVariantMap usageDetailsProviderDetail(const QString& providerId,
                                       const QVector<ProviderCostUsageSnapshot>& providers)
{
    QVariantMap detail;
    detail[QStringLiteral("providerId")] = providerId;
    detail[QStringLiteral("state")] = QStringLiteral("empty");
    detail[QStringLiteral("models")] = QVariantList();
    detail[QStringLiteral("daily")] = QVariantList();

    for (const auto& provider : providers) {
        if (provider.providerId != providerId) {
            continue;
        }
        detail[QStringLiteral("state")] = QStringLiteral("ready");
        detail[QStringLiteral("models")] = usageDetailsModelEntries(provider);
        detail[QStringLiteral("daily")] = usageDetailsDailyEntries(provider.snapshot);
        return detail;
    }

    return detail;
}

QVariantMap usageDetailsTokenRow(const QString& providerId,
                                 const ProviderCostUsageSnapshot* token,
                                 const QVariantMap& provider)
{
    const CostUsageSnapshot emptySnapshot;
    const CostUsageSnapshot& snapshot = token ? token->snapshot : emptySnapshot;
    return {
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("displayName"), usageDetailsDisplayNameFor(providerId, provider)},
        {QStringLiteral("brandColor"), usageDetailsBrandColorFor(providerId, provider)},
        {QStringLiteral("kind"), QStringLiteral("token")},
        {QStringLiteral("hasTokenData"), true},
        {QStringLiteral("hasDetailAvailable"), token && !token->modelSummary.isEmpty()},
        {QStringLiteral("sessionTokens"), snapshot.sessionTokens},
        {QStringLiteral("sessionCostUSD"), snapshot.sessionCostUSD},
        {QStringLiteral("last30DaysTokens"), snapshot.last30DaysTokens},
        {QStringLiteral("last30DaysCostUSD"), snapshot.last30DaysCostUSD},
        {QStringLiteral("daily"), usageDetailsDailyEntries(snapshot)},
        {QStringLiteral("enabled"), provider.isEmpty() || provider.value(QStringLiteral("enabled"), true).toBool()},
    };
}

QVariantMap usageDetailsQuotaRow(const QVariantMap& provider, const QString& kind)
{
    const QString providerId = provider.value(QStringLiteral("id")).toString();
    return {
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("displayName"), usageDetailsDisplayNameFor(providerId, provider)},
        {QStringLiteral("brandColor"), usageDetailsBrandColorFor(providerId, provider)},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("hasTokenData"), false},
        {QStringLiteral("hasDetailAvailable"), false},
        {QStringLiteral("sessionTokens"), 0},
        {QStringLiteral("sessionCostUSD"), 0.0},
        {QStringLiteral("last30DaysTokens"), 0},
        {QStringLiteral("last30DaysCostUSD"), 0.0},
        {QStringLiteral("daily"), QVariantList{}},
        {QStringLiteral("enabled"), provider.value(QStringLiteral("enabled"), true).toBool()},
    };
}

QVariantList buildUsageDetailsRows(const QVector<ProviderCostUsageSnapshot>& tokenProviders,
                                   const QVariantList& appProviders,
                                   int* tokenProviderCount)
{
    QHash<QString, QVariantMap> providerById;
    QVariantList rows;
    QSet<QString> seen;
    int tokenCount = 0;

    for (const QVariant& item : appProviders) {
        const QVariantMap provider = item.toMap();
        const QString id = provider.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            providerById.insert(id, provider);
        }
    }

    for (const auto& token : tokenProviders) {
        if (token.providerId.isEmpty()) {
            continue;
        }
        rows.append(usageDetailsTokenRow(token.providerId, &token, providerById.value(token.providerId)));
        seen.insert(token.providerId);
        ++tokenCount;
    }

    for (const QVariant& item : appProviders) {
        const QVariantMap provider = item.toMap();
        const QString id = provider.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || seen.contains(id)) {
            continue;
        }
        if (!provider.value(QStringLiteral("enabled"), true).toBool()) {
            continue;
        }

        const QString kind = usageDetailsProviderKind(provider);
        if (kind.isEmpty()) {
            continue;
        }

        if (kind == QLatin1String("token")) {
            rows.append(usageDetailsTokenRow(id, nullptr, provider));
            ++tokenCount;
        } else {
            rows.append(usageDetailsQuotaRow(provider, kind));
        }
    }

    if (tokenProviderCount) {
        *tokenProviderCount = tokenCount;
    }
    return rows;
}

} // namespace

bool CostUsageScanPlan::hasWork() const
{
    return scanClaude || scanCodex || scanPi || scanOpenCodeDB || scanOpenCodeGo;
}

CostUsageScanPlan CostUsageService::buildScanPlan(const QVector<QString>& enabledProviderIds,
                                                  const QSet<QString>& subscribedProviderIds)
{
    CostUsageScanPlan plan;
    plan.subscribedProviderIds = subscribedProviderIds;
    for (const auto& id : enabledProviderIds) {
        if (subscribedProviderIds.contains(id) || scansLocalUsageWithoutTokenAccount(id)) {
            plan.enabledProviderIds.insert(id);
        }
    }

    plan.scanClaude = plan.enabledProviderIds.contains(QStringLiteral("claude"));
    plan.scanCodex = plan.enabledProviderIds.contains(QStringLiteral("codex"));
    plan.scanPi = plan.scanClaude || plan.scanCodex;
    plan.scanOpenCodeGo = plan.enabledProviderIds.contains(QStringLiteral("opencodego"));
    plan.includeAllOpenCodeDBProviders = plan.enabledProviderIds.contains(QStringLiteral("opencode"));
    if (plan.scanOpenCodeGo) {
        plan.openCodeDBProviderIds.insert(QStringLiteral("opencodego"));
    }
    plan.scanOpenCodeDB = plan.includeAllOpenCodeDBProviders || !plan.openCodeDBProviderIds.isEmpty();
    return plan;
}

CostUsageRefreshPayload CostUsageService::refresh(const CostUsageScanPlan& plan)
{
    CostUsageCache& cache = CostUsageCache::instance();
    cache.load();

    CostUsageScanner scanner(&cache);

    const QDate today = QDate::currentDate();
    const QDate since = today.addDays(-29);

    CostUsageSnapshot claude;
    CostUsageSnapshot codex;
    CostUsageScanner::PiScanResult piResult;
    QHash<QString, CostUsageSnapshot> openCodeDBResults;
    CostUsageSnapshot openCodeGo;

    if (plan.scanClaude) {
        claude = scanner.scanClaude({}, since, today);
    }
    if (plan.scanCodex) {
        codex = scanner.scanCodex({}, since, today);
    }
    if (plan.scanPi) {
        piResult = scanner.scanPi(since, today);
    }
    if (plan.scanOpenCodeDB) {
        openCodeDBResults = scanner.scanOpenCodeDB(since, today, plan.openCodeDBProviderIds);
    }
    if (plan.scanOpenCodeGo) {
        openCodeGo = scanner.scanOpenCodeGo(since, today);
    }

    cache.save();

    QHash<QString, CostUsageSnapshot> perProvider;

    const CostUsageSnapshot mergedClaude = mergeCostUsageSnapshots({claude, piResult.claude});
    if (plan.enabledProviderIds.contains(QStringLiteral("claude"))
        && mergedClaude.last30DaysTokens > 0) {
        perProvider.insert(QStringLiteral("claude"), mergedClaude);
    }

    const CostUsageSnapshot mergedCodex = mergeCostUsageSnapshots({codex, piResult.codex});
    if (plan.enabledProviderIds.contains(QStringLiteral("codex"))
        && mergedCodex.last30DaysTokens > 0) {
        perProvider.insert(QStringLiteral("codex"), mergedCodex);
    }

    for (auto it = openCodeDBResults.constBegin(); it != openCodeDBResults.constEnd(); ++it) {
        const QString providerId = it.key();
        if (!plan.includeAllOpenCodeDBProviders && !plan.enabledProviderIds.contains(providerId)) {
            continue;
        }

        const CostUsageSnapshot snapshot = it.value();
        if (snapshot.last30DaysTokens <= 0) {
            continue;
        }

        if (perProvider.contains(providerId)) {
            mergeProviderSnapshot(perProvider[providerId], snapshot);
        } else {
            perProvider.insert(providerId, snapshot);
        }
    }

    if (plan.scanOpenCodeGo && openCodeGo.last30DaysTokens > 0) {
        perProvider.insert(QStringLiteral("opencodego"), openCodeGo);
    }

    QVector<CostUsageSnapshot> providerSnapshots;
    for (auto it = perProvider.constBegin(); it != perProvider.constEnd(); ++it) {
        providerSnapshots.append(it.value());
    }

    CostUsageRefreshPayload payload;
    payload.combined = mergeCostUsageSnapshots(providerSnapshots);
    payload.perProvider = perProvider;
    payload.allProviders = buildProviderSnapshots(perProvider);
    return payload;
}

QVariantMap CostUsageService::summaryData(const CostUsageSnapshot& snapshot)
{
    QVariantMap m;
    m[QStringLiteral("sessionTokens")] = snapshot.sessionTokens;
    m[QStringLiteral("sessionCostUSD")] = snapshot.sessionCostUSD;
    m[QStringLiteral("last30DaysTokens")] = snapshot.last30DaysTokens;
    m[QStringLiteral("last30DaysCostUSD")] = snapshot.last30DaysCostUSD;
    m[QStringLiteral("updatedAt")] = snapshot.updatedAt.toMSecsSinceEpoch();
    m[QStringLiteral("hasData")] = snapshot.last30DaysTokens > 0;

    QVariantList dailyList;
    for (const auto& d : snapshot.daily) {
        if (d.totalTokens() == 0) continue;
        QVariantMap dm;
        dm[QStringLiteral("date")] = d.date;
        dm[QStringLiteral("totalTokens")] = d.totalTokens();
        dm[QStringLiteral("costUSD")] = d.costUSD;

        QVariantList models;
        for (const auto& md : d.models) {
            QVariantMap mm;
            mm[QStringLiteral("name")] = md.modelName;
            mm[QStringLiteral("tokens")] = md.totalTokens();
            mm[QStringLiteral("costUSD")] = md.costUSD;
            models.append(mm);
        }
        dm[QStringLiteral("models")] = models;
        dailyList.append(dm);
    }
    m[QStringLiteral("daily")] = dailyList;
    return m;
}

QVariantList CostUsageService::providerRows(const QVector<ProviderCostUsageSnapshot>& providers)
{
    QVariantList result;
    for (const auto& pcs : providers) {
        QVariantMap m;
        m[QStringLiteral("providerId")] = pcs.providerId;
        m[QStringLiteral("sessionTokens")] = pcs.snapshot.sessionTokens;
        m[QStringLiteral("sessionCostUSD")] = pcs.snapshot.sessionCostUSD;
        m[QStringLiteral("last30DaysTokens")] = pcs.snapshot.last30DaysTokens;
        m[QStringLiteral("last30DaysCostUSD")] = pcs.snapshot.last30DaysCostUSD;

        QVariantList models;
        for (const auto& model : pcs.modelSummary) {
            QVariantMap mm;
            mm[QStringLiteral("name")] = model.modelName;
            mm[QStringLiteral("tokens")] = model.totalTokens();
            mm[QStringLiteral("costUSD")] = model.costUSD;
            models.append(mm);
        }
        m[QStringLiteral("models")] = models;

        QVariantList daily;
        for (const auto& d : pcs.snapshot.daily) {
            if (d.totalTokens() == 0) continue;
            QVariantMap dm;
            dm[QStringLiteral("date")] = d.date;
            dm[QStringLiteral("totalTokens")] = d.totalTokens();
            dm[QStringLiteral("costUSD")] = d.costUSD;
            daily.append(dm);
        }
        m[QStringLiteral("daily")] = daily;
        result.append(m);
    }
    return result;
}

CostUsageDetailsRowsPayload CostUsageService::detailsRows(
    const QVector<ProviderCostUsageSnapshot>& tokenProviders,
    const QVariantList& appProviders)
{
    CostUsageDetailsRowsPayload payload;
    payload.detailsRows = buildUsageDetailsRows(tokenProviders, appProviders, &payload.tokenProviderCount);
    return payload;
}

CostUsageProviderDetailPayload CostUsageService::providerDetail(
    const QString& providerId,
    const QVector<ProviderCostUsageSnapshot>& providers)
{
    CostUsageProviderDetailPayload payload;
    payload.providerId = providerId;
    payload.detail = usageDetailsProviderDetail(providerId, providers);
    return payload;
}

void CostUsageService::setShuttingDown(bool shuttingDown)
{
    CostUsageScanner::setShuttingDown(shuttingDown);
}
