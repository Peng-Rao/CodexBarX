#include "UsageDetailsViewModel.h"

#include "UsageStore.h"

#include <QHash>

UsageDetailsViewModel::UsageDetailsViewModel(UsageStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    m_syncTimer.setSingleShot(true);
    m_syncTimer.setInterval(16);
    connect(&m_syncTimer, &QTimer::timeout, this, &UsageDetailsViewModel::syncNow);

    if (!m_store) {
        return;
    }

    m_costUsageEnabled = m_store->costUsageEnabled();
    m_costUsageRefreshing = m_store->costUsageRefreshing();

    connect(m_store, &UsageStore::costUsageEnabledChanged,
            this, [this]() {
                syncCostFlags();
                scheduleSync();
            });
    connect(m_store, &UsageStore::costUsageRefreshingChanged,
            this, [this]() {
                syncCostFlags();
                scheduleSync();
            });
    connect(m_store, &UsageStore::costUsageChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::providerListModelChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::providerIDsChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::snapshotRevisionChanged,
            this, &UsageDetailsViewModel::scheduleSync);
    connect(m_store, &UsageStore::statusRevisionChanged,
            this, &UsageDetailsViewModel::scheduleSync);
}

void UsageDetailsViewModel::activate()
{
    if (!m_store) {
        return;
    }

    if (!m_active) {
        m_active = true;
        emit activeChanged();
    }

    m_store->ensureCostUsageEnabled();
    m_store->requestCostUsageViewData();
    m_store->requestProviderList();
    syncCostFlags();
    scheduleSync();
}

void UsageDetailsViewModel::deactivate()
{
    if (!m_active) {
        return;
    }

    m_active = false;
    emit activeChanged();
    if (m_store) {
        m_store->releaseCostUsageViewCaches();
    }
}

void UsageDetailsViewModel::refreshCostUsage()
{
    if (m_store) {
        m_store->refreshCostUsage();
    }
}

void UsageDetailsViewModel::scheduleSync()
{
    if (!m_active || !m_store) {
        return;
    }
    if (!m_syncTimer.isActive()) {
        m_syncTimer.start();
    }
}

void UsageDetailsViewModel::syncNow()
{
    if (!m_active || !m_store) {
        return;
    }

    syncCostFlags();

    const QVariantMap nextCostData = m_store->costUsageData();
    const QVariantList tokenProviders = m_store->providerCostUsageList();
    const QVariantList appProviders = m_store->providerList();
    const QVariantList nextRows = buildProviderRows(tokenProviders, appProviders);

    if (m_costData != nextCostData) {
        m_costData = nextCostData;
        emit costDataChanged();
    }

    if (m_providerRows != nextRows) {
        m_providerRows = nextRows;
        int nextTokenProviderCount = 0;
        for (const QVariant& item : m_providerRows) {
            if (item.toMap().value(QStringLiteral("hasTokenData")).toBool()) {
                ++nextTokenProviderCount;
            }
        }
        m_tokenProviderCount = nextTokenProviderCount;
        emit providerRowsChanged();
    }
}

void UsageDetailsViewModel::syncCostFlags()
{
    if (!m_store) {
        return;
    }

    const bool nextEnabled = m_store->costUsageEnabled();
    if (m_costUsageEnabled != nextEnabled) {
        m_costUsageEnabled = nextEnabled;
        emit costUsageEnabledChanged();
    }

    const bool nextRefreshing = m_store->costUsageRefreshing();
    if (m_costUsageRefreshing != nextRefreshing) {
        m_costUsageRefreshing = nextRefreshing;
        emit costUsageRefreshingChanged();
    }
}

QVariantList UsageDetailsViewModel::buildProviderRows(const QVariantList& tokenProviders,
                                                      const QVariantList& appProviders) const
{
    QHash<QString, QVariantMap> providerById;
    QVariantList rows;
    QSet<QString> seen;

    for (const QVariant& item : appProviders) {
        const QVariantMap provider = item.toMap();
        const QString id = provider.value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) {
            providerById.insert(id, provider);
        }
    }

    for (const QVariant& item : tokenProviders) {
        const QVariantMap token = item.toMap();
        const QString tokenId = token.value(QStringLiteral("providerId"),
                                            token.value(QStringLiteral("id"))).toString();
        if (tokenId.isEmpty()) {
            continue;
        }
        rows.append(makeTokenRow(tokenId, token, providerById.value(tokenId)));
        seen.insert(tokenId);
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

        const QString kind = providerUsageKind(provider);
        if (kind.isEmpty()) {
            continue;
        }

        if (kind == QLatin1String("token")) {
            rows.append(makeTokenRow(id, QVariantMap(), provider));
        } else {
            rows.append(makeQuotaRow(provider, kind));
        }
    }

    return rows;
}

QVariantMap UsageDetailsViewModel::makeTokenRow(const QString& providerId,
                                                const QVariantMap& token,
                                                const QVariantMap& provider) const
{
    return {
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("displayName"), displayNameFor(providerId, provider)},
        {QStringLiteral("brandColor"), brandColorFor(providerId, provider)},
        {QStringLiteral("kind"), QStringLiteral("token")},
        {QStringLiteral("hasTokenData"), true},
        {QStringLiteral("sessionTokens"), token.value(QStringLiteral("sessionTokens")).toDouble()},
        {QStringLiteral("sessionCostUSD"), token.value(QStringLiteral("sessionCostUSD")).toDouble()},
        {QStringLiteral("last30DaysTokens"), token.value(QStringLiteral("last30DaysTokens")).toDouble()},
        {QStringLiteral("last30DaysCostUSD"), token.value(QStringLiteral("last30DaysCostUSD")).toDouble()},
        {QStringLiteral("models"), token.value(QStringLiteral("models")).toList()},
        {QStringLiteral("daily"), token.value(QStringLiteral("daily")).toList()},
        {QStringLiteral("enabled"), provider.isEmpty() || provider.value(QStringLiteral("enabled"), true).toBool()},
    };
}

QVariantMap UsageDetailsViewModel::makeQuotaRow(const QVariantMap& provider, const QString& kind) const
{
    const QString providerId = provider.value(QStringLiteral("id")).toString();
    return {
        {QStringLiteral("providerId"), providerId},
        {QStringLiteral("displayName"), displayNameFor(providerId, provider)},
        {QStringLiteral("brandColor"), brandColorFor(providerId, provider)},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("hasTokenData"), false},
        {QStringLiteral("sessionTokens"), 0},
        {QStringLiteral("sessionCostUSD"), 0.0},
        {QStringLiteral("last30DaysTokens"), 0},
        {QStringLiteral("last30DaysCostUSD"), 0.0},
        {QStringLiteral("models"), QVariantList{}},
        {QStringLiteral("daily"), QVariantList{}},
        {QStringLiteral("enabled"), provider.value(QStringLiteral("enabled"), true).toBool()},
    };
}

QString UsageDetailsViewModel::providerUsageKind(const QVariantMap& provider) const
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

QString UsageDetailsViewModel::displayNameFor(const QString& providerId, const QVariantMap& provider) const
{
    const QString providerName = provider.value(QStringLiteral("name")).toString();
    if (!providerName.isEmpty()) {
        return providerName;
    }
    static const QHash<QString, QString> names = {
        {QStringLiteral("codex"), QStringLiteral("Codex")},
        {QStringLiteral("claude"), QStringLiteral("Claude")},
        {QStringLiteral("opencodego"), QStringLiteral("OpenCode Go")},
        {QStringLiteral("kimi"), QStringLiteral("Kimi")},
        {QStringLiteral("kimik2"), QStringLiteral("Kimi K2")},
        {QStringLiteral("copilot"), QStringLiteral("Copilot")},
        {QStringLiteral("cursor"), QStringLiteral("Cursor")},
    };
    return names.value(providerId, providerId);
}

QString UsageDetailsViewModel::brandColorFor(const QString& providerId, const QVariantMap& provider) const
{
    const QString providerColor = provider.value(QStringLiteral("brandColor")).toString();
    return providerColor.isEmpty() ? fallbackBrandColor(providerId) : providerColor;
}

QString UsageDetailsViewModel::fallbackBrandColor(const QString& providerId) const
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
