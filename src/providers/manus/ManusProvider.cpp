#include "ManusProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/CookieImporter.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

ManusProvider::ManusProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> ManusProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new ManusWebStrategy(this) };
}

ManusWebStrategy::ManusWebStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString ManusWebStrategy::resolveCookieHeader(const ProviderFetchContext& ctx) {
    // 1. 手动输入的 cookie
    if (ctx.manualCookieHeader.has_value() && !ctx.manualCookieHeader->isEmpty()) {
        return *ctx.manualCookieHeader;
    }

    // 2. 多账户凭证
    if (ctx.accountCredentials.web.has_value() && !ctx.accountCredentials.web->cookieValue.toString().isEmpty()) {
        return "session_id=" + ctx.accountCredentials.web->cookieValue.toString();
    }

    // 3. 浏览器导入
    QStringList domains = {"manus.im", "api.manus.im"};
    for (auto browser : CookieImporter::importOrder()) {
        if (!CookieImporter::isBrowserInstalled(browser)) continue;
        auto cookies = CookieImporter::importCookies(browser, domains);
        if (cookies.isEmpty()) continue;
        QStringList parts;
        for (const auto& c : cookies) {
            parts.append(c.name() + "=" + c.value());
        }
        return parts.join("; ");
    }
    return {};
}

bool ManusWebStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveCookieHeader(ctx).isEmpty();
}

bool ManusWebStrategy::shouldFallback(const ProviderFetchResult& result,
                                       const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult ManusWebStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "web";

    QString cookieHeader = resolveCookieHeader(ctx);
    if (cookieHeader.isEmpty()) {
        result.success = false;
        result.errorMessage = "Manus session cookie not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Cookie"] = cookieHeader;
    headers["Accept"] = "application/json";
    headers["Content-Type"] = "application/json";
    headers["Connect-Protocol-Version"] = "1";

    // 发送 POST 请求获取额度
    QJsonObject body;
    auto [json, httpStatus] = NetworkManager::instance().postJsonSyncWithStatus(
        QUrl("https://api.manus.im/user.v1.UserService/GetAvailableCredits"),
        body, headers, ctx.networkTimeoutMs);

    result.httpStatus = httpStatus;

    if (json.isEmpty() && httpStatus != 200) {
        result.success = false;
        result.errorMessage = httpStatus == 401 ? "Authentication failed. Please re-login."
            : QString("HTTP error: %1").arg(httpStatus);
        return result;
    }

    return parseResponse(json);
}

ProviderFetchResult ManusWebStrategy::parseResponse(const QJsonObject& json) {
    ProviderFetchResult result;
    result.strategyID = "manus.web";
    result.strategyKind = ProviderFetchKind::Web;
    result.sourceLabel = "web";

    if (json.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty response from Manus API";
        return result;
    }

    double totalCredits = json["totalCredits"].toDouble(0.0);
    double freeCredits = json["freeCredits"].toDouble(0.0);
    double periodicCredits = json["periodicCredits"].toDouble(0.0);
    double refreshCredits = json["refreshCredits"].toDouble(0.0);
    double maxRefreshCredits = json["maxRefreshCredits"].toDouble(0.0);
    double proMonthlyCredits = json["proMonthlyCredits"].toDouble(0.0);

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::manus;
    snap.identity = identity;

    // 主窗口：Pro Monthly Credits 或 Total Credits
    if (proMonthlyCredits > 0) {
        RateWindow primary;
        primary.usedPercent = 0;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 Pro credits")
            .arg(proMonthlyCredits, 0, 'f', 1);
        snap.primary = primary;
    } else if (totalCredits > 0) {
        RateWindow primary;
        primary.usedPercent = 0;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 total credits")
            .arg(totalCredits, 0, 'f', 1);
        snap.primary = primary;
    }

    // 次窗口：Refresh Credits
    if (refreshCredits > 0 && maxRefreshCredits > 0) {
        RateWindow secondary;
        secondary.usedPercent = ((maxRefreshCredits - refreshCredits) / maxRefreshCredits) * 100.0;
        secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 / %2 refresh credits")
            .arg(refreshCredits, 0, 'f', 1)
            .arg(maxRefreshCredits, 0, 'f', 1);
        snap.secondary = secondary;
    }

    // 额外信息
    if (freeCredits > 0 || periodicCredits > 0) {
        CreditsSnapshot credits;
        credits.remaining = freeCredits + periodicCredits;
        result.credits = credits;
    }

    result.usage = snap;
    result.success = true;

    return result;
}
