#include "MiMoProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/CookieImporter.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

MiMoProvider::MiMoProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> MiMoProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new MiMoWebStrategy(this) };
}

MiMoWebStrategy::MiMoWebStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString MiMoWebStrategy::resolveCookieHeader(const ProviderFetchContext& ctx) {
    if (ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
        return *ctx.manualCookieHeader;
    }
    if (ctx.accountCredentials.web.has_value() && !ctx.accountCredentials.web->cookieValue.toString().isEmpty()) {
        return ctx.accountCredentials.web->cookieValue.toString();
    }
    QStringList domains = {"xiaomimimo.com", "platform.xiaomimimo.com"};
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

bool MiMoWebStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveCookieHeader(ctx).isEmpty();
}

bool MiMoWebStrategy::shouldFallback(const ProviderFetchResult& result,
                                      const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult MiMoWebStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "web";

    QString cookieHeader = resolveCookieHeader(ctx);
    if (cookieHeader.isEmpty()) {
        result.success = false;
        result.errorMessage = "MiMo session cookie not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Cookie"] = cookieHeader;
    headers["Accept"] = "application/json";

    // 获取余额
    QJsonObject balanceJson = NetworkManager::instance().getJsonSync(
        QUrl("https://platform.xiaomimimo.com/api/v1/balance"), headers, ctx.networkTimeoutMs);

    // 获取计划详情
    QJsonObject planJson = NetworkManager::instance().getJsonSync(
        QUrl("https://platform.xiaomimimo.com/api/v1/tokenPlan/detail"), headers, ctx.networkTimeoutMs);

    // 获取用量
    QJsonObject usageJson = NetworkManager::instance().getJsonSync(
        QUrl("https://platform.xiaomimimo.com/api/v1/tokenPlan/usage"), headers, ctx.networkTimeoutMs);

    return parseResponse(balanceJson, planJson, usageJson);
}

ProviderFetchResult MiMoWebStrategy::parseResponse(const QJsonObject& balanceJson,
                                                    const QJsonObject& planJson,
                                                    const QJsonObject& usageJson) {
    ProviderFetchResult result;
    result.strategyID = "mimo.web";
    result.strategyKind = ProviderFetchKind::Web;
    result.sourceLabel = "web";

    if (balanceJson.isEmpty() && usageJson.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty response from MiMo API";
        return result;
    }

    double balance = balanceJson["balance"].toString().toDouble();
    QString currency = balanceJson["currency"].toString("CNY");

    int tokenUsed = usageJson["used"].toInt(0);
    int tokenLimit = usageJson["limit"].toInt(0);
    double tokenPercent = usageJson["percent"].toDouble();

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::mimo;
    snap.identity = identity;

    // 主窗口：Token 使用率
    if (tokenLimit > 0) {
        RateWindow primary;
        primary.usedPercent = tokenPercent > 0 ? tokenPercent : (tokenUsed * 100.0 / tokenLimit);
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 / %2 tokens")
            .arg(tokenUsed).arg(tokenLimit);
        snap.primary = primary;
    }

    // 次窗口：余额
    if (balance > 0) {
        RateWindow secondary;
        secondary.usedPercent = 0;
        QString symbol = (currency == "CNY") ? "¥" : currency;
        secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1%2 balance")
            .arg(symbol).arg(balance, 0, 'f', 2);
        snap.secondary = secondary;
    }

    result.usage = snap;
    result.success = true;

    return result;
}
