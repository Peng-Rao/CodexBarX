#include "ClaudeProvider.h"
#include "ClaudeCLISession.h"
#include "ClaudeStatusProbe.h"
#include "ClaudeSourcePlanner.h"
#include "ClaudeCredentialRouting.h"
#include "ClaudeWebExtraFetcher.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/CookieImporter.h"
#include "../../models/ClaudeUsageSnapshot.h"
#include "../../account/TokenAccountStore.h"

#include <QJsonDocument>
#include <QJsonArray>

namespace {

std::optional<QString> extractClaudeSessionKey(const QVector<QNetworkCookie>& cookies)
{
    for (const auto& cookie : cookies) {
        if (cookie.name() == "sessionKey") {
            QString value = QString::fromUtf8(cookie.value()).trimmed();
            if (value.startsWith("sk-ant-")) return value;
        }
    }
    return std::nullopt;
}

std::optional<QString> extractClaudeSessionKeyFromHeader(const QString& header)
{
    for (const auto& pair : header.split(';', Qt::SkipEmptyParts)) {
        QString trimmed = pair.trimmed();
        int eq = trimmed.indexOf('=');
        if (eq <= 0) continue;

        QString name = trimmed.left(eq).trimmed();
        QString value = trimmed.mid(eq + 1).trimmed();
        if (name == "sessionKey" && value.startsWith("sk-ant-")) {
            return value;
        }
    }
    return std::nullopt;
}

bool hasStoredClaudeSessionCookie()
{
    QStringList domains = {QStringLiteral("claude.ai")};
    for (auto browser : CookieImporter::importOrder()) {
        if (!CookieImporter::isBrowserInstalled(browser)) continue;
        if (extractClaudeSessionKey(CookieImporter::importCookies(browser, domains)).has_value()) {
            return true;
        }
    }
    return false;
}

} // namespace

ClaudeProvider::ClaudeProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> ClaudeProvider::createStrategies(const ProviderFetchContext& ctx) {
    // Build planning input
    ClaudeSourcePlanningInput input;
    input.selectedSource = ClaudeSourcePlanner::dataSourceFromString(ctx.settings.get("sourceMode").toString());

    // Check for TokenAccount credentials first
    bool hasTokenAccountOAuth = ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::OAuth);
    bool hasTokenAccountWeb = ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::Web);

    // Fall back to system credentials if no TokenAccount
    input.hasOAuthCredentials = hasTokenAccountOAuth || ClaudeOAuthCredentials::load(ctx.env).has_value();
    input.hasCLI = ClaudeCLISession::isClaudeInstalled();
    input.hasWebSession = hasTokenAccountWeb || hasWebSessionCookie(ctx);
    input.isCLIRuntime = !ctx.isAppRuntime;

    // Resolve execution plan
    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    // Build strategies from plan
    QVector<IFetchStrategy*> strategies;
    for (const auto& step : plan.orderedSteps) {
        switch (step.dataSource) {
        case ClaudeDataSource::OAuth:
            strategies.append(new ClaudeOAuthStrategy());
            break;
        case ClaudeDataSource::CLI:
            strategies.append(new ClaudeCLIStrategy());
            break;
        case ClaudeDataSource::Web:
            strategies.append(new ClaudeWebStrategy());
            break;
        default:
            break;
        }
    }

    return strategies;
}

bool ClaudeProvider::hasWebSessionCookie(const ProviderFetchContext& ctx) const {
    if (ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
        return extractClaudeSessionKeyFromHeader(*ctx.manualCookieHeader).has_value();
    }
    return hasStoredClaudeSessionCookie();
}

ClaudeOAuthStrategy::ClaudeOAuthStrategy(QObject* parent) : IFetchStrategy(parent) {}

bool ClaudeOAuthStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    // Check TokenAccount credentials first
    if (ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::OAuth)) {
        return true;
    }
    // Fall back to system OAuth credentials
    return ClaudeOAuthCredentials::load(ctx.env).has_value();
}

bool ClaudeOAuthStrategy::shouldFallback(const ProviderFetchResult& result,
                                          const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult ClaudeOAuthStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "oauth";

    QString accessToken;
    std::optional<QString> rateLimitTier;

    // Try TokenAccount credentials first
    if (ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::OAuth) &&
        ctx.accountCredentials.oauth.has_value()) {
        accessToken = ctx.accountCredentials.oauth->accessToken.toString();
    }

    // Fall back to system credentials
    if (accessToken.isEmpty()) {
        auto credsOpt = ClaudeOAuthCredentials::load(ctx.env);
        if (!credsOpt.has_value()) {
            result.success = false;
            result.errorMessage = "Claude OAuth credentials not found. Run `claude` to authenticate.";
            return result;
        }
        accessToken = credsOpt->accessToken;
        rateLimitTier = credsOpt->rateLimitTier;
    }

    QHash<QString, QString> headers;
    headers["Authorization"] = "Bearer " + accessToken;
    headers["Accept"] = "application/json";
    headers["Content-Type"] = "application/json";
    headers["anthropic-beta"] = "oauth-2025-04-20";
    headers["User-Agent"] = "claude-code/2.1.0";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://api.anthropic.com/api/oauth/usage"), headers, ctx.networkTimeoutMs);

    if (json.isEmpty()) {
        result.success = false;
        result.errorMessage = "empty or invalid response from Claude OAuth API";
        return result;
    }

    if (json.contains("error")) {
        result.success = false;
        result.errorMessage = json.value("error").toObject().value("message").toString("OAuth error");
        return result;
    }

    ClaudeUsageSnapshot snap = ClaudeUsageSnapshot::fromOAuthJson(json);
    if (!snap.isValid()) {
        result.success = false;
        result.errorMessage = "no usage data in Claude OAuth response";
        return result;
    }

    if (rateLimitTier.has_value()) {
        snap.loginMethod = claudePlanDisplayName(claudePlanFromRateLimitTier(*rateLimitTier));
    }

    // Fetch Web extras if enabled and available
    bool webExtrasEnabled = ctx.settings.get("webExtrasEnabled").toBool();
    if (webExtrasEnabled && ctx.isAppRuntime) {
        QString sessionKey;

        // Try TokenAccount credentials first
        if (ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::Web) &&
            ctx.accountCredentials.web.has_value()) {
            sessionKey = ctx.accountCredentials.web->cookieValue.toString();
        }

        // Fall back to manual cookie header
        if (sessionKey.isEmpty() && ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
            sessionKey = extractClaudeSessionKeyFromHeader(*ctx.manualCookieHeader).value_or(QString());
        }

        // Fall back to browser cookies
        if (sessionKey.isEmpty()) {
            QStringList domains = {"claude.ai"};
            for (auto browser : CookieImporter::importOrder()) {
                if (!CookieImporter::isBrowserInstalled(browser)) continue;
                QVector<QNetworkCookie> cookies = CookieImporter::importCookies(browser, domains);
                auto key = extractClaudeSessionKey(cookies);
                if (key.has_value()) {
                    sessionKey = *key;
                    break;
                }
            }
        }

        if (!sessionKey.isEmpty()) {
            QString orgId = ClaudeWebStrategy::fetchOrgId(sessionKey, ctx.networkTimeoutMs);
            if (!orgId.isEmpty()) {
                auto extras = ClaudeWebExtraFetcher::instance().fetch(sessionKey, orgId, ctx.networkTimeoutMs);
                if (!extras.extraRateWindows.isEmpty()) {
                    UsageSnapshot usage = snap.toUsageSnapshot();
                    if (usage.extraRateWindows.isEmpty()) {
                        usage.extraRateWindows = extras.extraRateWindows;
                    }
                    if (!usage.providerCost.has_value() && extras.providerCost.has_value()) {
                        usage.providerCost = extras.providerCost;
                    }
                    result.usage = usage;
                    result.success = true;
                    return result;
                }
            }
        }
    }

    result.usage = snap.toUsageSnapshot();
    result.success = true;
    return result;
}

ClaudeWebStrategy::ClaudeWebStrategy(QObject* parent) : IFetchStrategy(parent) {}

bool ClaudeWebStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    // Check TokenAccount credentials first
    if (ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::Web)) {
        return true;
    }
    // Check manual cookie header
    if (ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
        return extractClaudeSessionKeyFromHeader(*ctx.manualCookieHeader).has_value();
    }
    // Check stored browser cookies
    return hasStoredClaudeSessionCookie();
}

bool ClaudeWebStrategy::shouldFallback(const ProviderFetchResult& result,
                                        const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

std::optional<QString> ClaudeWebStrategy::extractSessionKey(const QVector<QNetworkCookie>& cookies) {
    return extractClaudeSessionKey(cookies);
}

QString ClaudeWebStrategy::fetchOrgId(const QString& sessionKey, int timeoutMs) {
    QHash<QString, QString> headers;
    headers["Cookie"] = "sessionKey=" + sessionKey;
    headers["Accept"] = "application/json";

    QString response = NetworkManager::instance().getStringSync(
        QUrl("https://claude.ai/api/organizations"), headers, timeoutMs);

    if (response.isEmpty()) return {};

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) return {};

    QJsonArray orgs;
    if (doc.isArray()) {
        orgs = doc.array();
    } else if (doc.isObject()) {
        QJsonObject obj = doc.object();
        if (obj.contains("uuid")) return obj.value("uuid").toString();
        QJsonValue data = obj.value("data");
        if (data.isArray()) orgs = data.toArray();
    }

    for (const auto& orgVal : orgs) {
        QJsonObject org = orgVal.toObject();
        QJsonArray caps = org.value("capabilities").toArray();
        bool hasChat = false;
        for (const auto& c : caps) {
            if (c.toString().toLower() == "chat") { hasChat = true; break; }
        }
        if (hasChat) return org.value("uuid").toString();
    }

    if (!orgs.isEmpty()) return orgs[0].toObject().value("uuid").toString();
    return {};
}

ClaudeUsageSnapshot ClaudeWebStrategy::fetchUsageData(const QString& orgId, const QString& sessionKey, int timeoutMs) {
    QHash<QString, QString> headers;
    headers["Cookie"] = "sessionKey=" + sessionKey;
    headers["Accept"] = "application/json";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://claude.ai/api/organizations/" + orgId + "/usage"), headers, timeoutMs);

    return ClaudeUsageSnapshot::fromWebJson(json);
}

std::optional<ProviderCostSnapshot> ClaudeWebStrategy::fetchOverageCost(
    const QString& orgId, const QString& sessionKey, int timeoutMs)
{
    QHash<QString, QString> headers;
    headers["Cookie"] = "sessionKey=" + sessionKey;
    headers["Accept"] = "application/json";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://claude.ai/api/organizations/" + orgId + "/overage_spend_limit"), headers, timeoutMs);

    if (json.isEmpty()) return std::nullopt;
    if (!json.value("is_enabled").toBool(false)) return std::nullopt;

    double usedCredits = json.value("used_credits").toDouble(0);
    double monthlyLimit = json.value("monthly_credit_limit").toDouble(0);
    QString currency = json.value("currency").toString("USD");

    if (usedCredits <= 0 && monthlyLimit <= 0) return std::nullopt;

    ProviderCostSnapshot cost;
    cost.used = usedCredits / 100.0;
    cost.limit = monthlyLimit / 100.0;
    cost.currencyCode = currency;
    cost.period = "Monthly";
    cost.updatedAt = QDateTime::currentDateTime();
    return cost;
}

std::optional<QString> ClaudeWebStrategy::fetchAccountEmail(const QString& sessionKey, int timeoutMs) {
    QHash<QString, QString> headers;
    headers["Cookie"] = "sessionKey=" + sessionKey;
    headers["Accept"] = "application/json";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://claude.ai/api/account"), headers, timeoutMs);

    if (json.isEmpty()) return std::nullopt;
    QString email = json.value("email_address").toString().trimmed();
    if (email.isEmpty()) email = json.value("emailAddress").toString().trimmed();
    return email.isEmpty() ? std::optional<QString>{} : email;
}

ProviderFetchResult ClaudeWebStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "web";

    QString sessionKey;

    // Try TokenAccount credentials first
    if (ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::Web) &&
        ctx.accountCredentials.web.has_value()) {
        sessionKey = ctx.accountCredentials.web->cookieValue.toString();
    }

    // Fall back to manual cookie header
    if (sessionKey.isEmpty() && ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
        sessionKey = extractClaudeSessionKeyFromHeader(*ctx.manualCookieHeader).value_or(QString());
    }

    // Fall back to browser cookies
    if (sessionKey.isEmpty()) {
        QStringList domains = {"claude.ai"};
        for (auto browser : CookieImporter::importOrder()) {
            if (!CookieImporter::isBrowserInstalled(browser)) continue;
            QVector<QNetworkCookie> cookies = CookieImporter::importCookies(browser, domains);
            auto key = extractSessionKey(cookies);
            if (key.has_value()) {
                sessionKey = *key;
                break;
            }
        }
    }

    if (sessionKey.isEmpty()) {
        result.success = false;
        result.errorMessage = "No Claude session key found in browser cookies.";
        return result;
    }

    QString orgId = fetchOrgId(sessionKey, ctx.networkTimeoutMs);
    if (orgId.isEmpty()) {
        result.success = false;
        result.errorMessage = "No Claude organization found.";
        return result;
    }

    ClaudeUsageSnapshot snap = fetchUsageData(orgId, sessionKey, ctx.networkTimeoutMs);
    if (!snap.isValid()) {
        result.success = false;
        result.errorMessage = "No usage data from Claude web API.";
        return result;
    }

    auto overage = fetchOverageCost(orgId, sessionKey, ctx.networkTimeoutMs);
    if (overage.has_value()) {
        snap.extraUsage = ClaudeExtraUsage{
            true,
            overage->limit * 100.0,
            overage->used * 100.0,
            std::nullopt,
            overage->currencyCode
        };
    }

    auto email = fetchAccountEmail(sessionKey, ctx.networkTimeoutMs);
    if (email.has_value()) snap.accountEmail = email;

    result.usage = snap.toUsageSnapshot();
    result.success = true;
    return result;
}

// --- ClaudeCLIStrategy ---

ClaudeCLIStrategy::ClaudeCLIStrategy(QObject* parent) : IFetchStrategy(parent) {}

bool ClaudeCLIStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return ClaudeCLISession::isClaudeInstalled();
}

bool ClaudeCLIStrategy::shouldFallback(const ProviderFetchResult& result,
                                        const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult ClaudeCLIStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "cli";

    // 1. Resolve binary path
    QString claudeBinary = ClaudeCLISession::resolveBinaryPath();
    if (claudeBinary.isEmpty()) {
        result.success = false;
        result.errorMessage = "Claude CLI not found in PATH. Install from https://claude.ai/download";
        return result;
    }

    // 2. Create session and capture output
    ClaudeCLISession session;
    session.setEnvironment(ctx.env);
    session.setTimeout(ctx.networkTimeoutMs);

    auto captureResult = session.captureUsageAndStatus(ctx.networkTimeoutMs);
    if (!captureResult.success) {
        result.success = false;
        result.errorMessage = captureResult.errorMessage;
        return result;
    }

    // 3. Parse output
    auto parseResult = ClaudeStatusProbe::parse(captureResult.usageOutput,
                                                captureResult.statusOutput);
    if (!parseResult.success) {
        result.success = false;
        result.errorMessage = parseResult.errorMessage;
        return result;
    }

    // 4. Convert to UsageSnapshot
    ClaudeUsageSnapshot snap = ClaudeUsageSnapshot::fromCLIOutput(parseResult.snapshot);
    if (!snap.isValid()) {
        result.success = false;
        result.errorMessage = "No valid usage data from Claude CLI";
        return result;
    }

    // 5. Fetch Web extras if enabled and available
    bool webExtrasEnabled = ctx.settings.get("webExtrasEnabled").toBool();
    if (webExtrasEnabled && ctx.isAppRuntime) {
        QString sessionKey;

        // Try TokenAccount credentials first
        if (ctx.accountCredentials.hasCredentialsFor(ProviderFetchKind::Web) &&
            ctx.accountCredentials.web.has_value()) {
            sessionKey = ctx.accountCredentials.web->cookieValue.toString();
        }

        // Fall back to manual cookie header
        if (sessionKey.isEmpty() && ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
            sessionKey = extractClaudeSessionKeyFromHeader(*ctx.manualCookieHeader).value_or(QString());
        }

        // Fall back to browser cookies
        if (sessionKey.isEmpty()) {
            QStringList domains = {"claude.ai"};
            for (auto browser : CookieImporter::importOrder()) {
                if (!CookieImporter::isBrowserInstalled(browser)) continue;
                QVector<QNetworkCookie> cookies = CookieImporter::importCookies(browser, domains);
                auto key = extractClaudeSessionKey(cookies);
                if (key.has_value()) {
                    sessionKey = *key;
                    break;
                }
            }
        }

        if (!sessionKey.isEmpty()) {
            QString orgId = ClaudeWebStrategy::fetchOrgId(sessionKey, ctx.networkTimeoutMs);
            if (!orgId.isEmpty()) {
                auto extras = ClaudeWebExtraFetcher::instance().fetch(sessionKey, orgId, ctx.networkTimeoutMs);
                if (!extras.extraRateWindows.isEmpty()) {
                    UsageSnapshot usage = snap.toUsageSnapshot();
                    if (usage.extraRateWindows.isEmpty()) {
                        usage.extraRateWindows = extras.extraRateWindows;
                    }
                    if (!usage.providerCost.has_value() && extras.providerCost.has_value()) {
                        usage.providerCost = extras.providerCost;
                    }
                    result.usage = usage;
                    result.success = true;
                    return result;
                }
            }
        }
    }

    result.usage = snap.toUsageSnapshot();
    result.success = true;
    return result;
}
