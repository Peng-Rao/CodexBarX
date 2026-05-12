#include "CommandCodeProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/CookieImporter.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

CommandCodeProvider::CommandCodeProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> CommandCodeProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new CommandCodeWebStrategy(this) };
}

CommandCodeWebStrategy::CommandCodeWebStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString CommandCodeWebStrategy::resolveCookieHeader(const ProviderFetchContext& ctx) {
    if (ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
        return *ctx.manualCookieHeader;
    }
    if (ctx.accountCredentials.web.has_value() && !ctx.accountCredentials.web->cookieValue.toString().isEmpty()) {
        return ctx.accountCredentials.web->cookieValue.toString();
    }
    QStringList domains = {"commandcode.ai", "api.commandcode.ai"};
    for (auto browser : CookieImporter::importOrder()) {
        if (!CookieImporter::isBrowserInstalled(browser)) continue;
        auto cookies = CookieImporter::importCookies(browser, domains);
        if (cookies.isEmpty()) continue;
        QStringList parts;
        for (const auto& c : cookies) parts.append(c.name() + "=" + c.value());
        return parts.join("; ");
    }
    return {};
}

bool CommandCodeWebStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveCookieHeader(ctx).isEmpty();
}

bool CommandCodeWebStrategy::shouldFallback(const ProviderFetchResult& result,
                                             const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult CommandCodeWebStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "web";

    QString cookieHeader = resolveCookieHeader(ctx);
    if (cookieHeader.isEmpty()) {
        result.success = false;
        result.errorMessage = "CommandCode session cookie not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Cookie"] = cookieHeader;
    headers["Accept"] = "application/json";

    // 获取额度信息
    QJsonObject creditsJson = NetworkManager::instance().getJsonSync(
        QUrl("https://api.commandcode.ai/internal/billing/credits"), headers, ctx.networkTimeoutMs);

    // 获取订阅信息
    QJsonObject subJson = NetworkManager::instance().getJsonSync(
        QUrl("https://api.commandcode.ai/internal/billing/subscriptions"), headers, ctx.networkTimeoutMs);

    return parseResponse(creditsJson, subJson);
}

ProviderFetchResult CommandCodeWebStrategy::parseResponse(const QJsonObject& creditsJson, const QJsonObject& subJson) {
    ProviderFetchResult result;
    result.strategyID = "commandcode.web";
    result.strategyKind = ProviderFetchKind::Web;
    result.sourceLabel = "web";

    if (creditsJson.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty response from CommandCode API";
        return result;
    }

    double monthlyCredits = creditsJson["monthlyCredits"].toDouble(0.0);
    double purchasedCredits = creditsJson["purchasedCredits"].toDouble(0.0);
    double premiumMonthlyCredits = creditsJson["premiumMonthlyCredits"].toDouble(0.0);

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::commandcode;
    snap.identity = identity;

    // 主窗口：月度额度
    if (monthlyCredits > 0 || premiumMonthlyCredits > 0) {
        RateWindow primary;
        primary.usedPercent = 0;
        double total = monthlyCredits + premiumMonthlyCredits;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "$%1 monthly credits")
            .arg(total, 0, 'f', 2);
        snap.primary = primary;
    }

    // 次窗口：购买的额度
    if (purchasedCredits > 0) {
        RateWindow secondary;
        secondary.usedPercent = 0;
        secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "$%1 purchased")
            .arg(purchasedCredits, 0, 'f', 2);
        snap.secondary = secondary;
    }

    result.usage = snap;
    result.success = true;

    return result;
}
