#include "CrofProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

CrofProvider::CrofProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> CrofProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new CrofAPIStrategy(this) };
}

CrofAPIStrategy::CrofAPIStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString CrofAPIStrategy::resolveApiKey(const ProviderFetchContext& ctx) {
    if (ctx.env.contains("CROF_API_KEY")) return ctx.env["CROF_API_KEY"];
    if (ctx.accountCredentials.api.has_value() && ctx.accountCredentials.api->isValid()) {
        return ctx.accountCredentials.api->apiKey.toString().trimmed();
    }
    auto cred = ProviderCredentialStore::read("com.codexbarx.apikey.crof");
    if (cred.has_value()) return QString::fromUtf8(cred.value());
    return {};
}

bool CrofAPIStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveApiKey(ctx).isEmpty();
}

bool CrofAPIStrategy::shouldFallback(const ProviderFetchResult& result,
                                      const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult CrofAPIStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "api";

    QString apiKey = resolveApiKey(ctx);
    if (apiKey.isEmpty()) {
        result.success = false;
        result.errorMessage = "Crof API key not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Authorization"] = "Bearer " + apiKey;
    headers["Accept"] = "application/json";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://crof.ai/usage_api/"), headers, ctx.networkTimeoutMs);

    return parseResponse(json);
}

ProviderFetchResult CrofAPIStrategy::parseResponse(const QJsonObject& json) {
    ProviderFetchResult result;
    result.strategyID = "crof.api";
    result.strategyKind = ProviderFetchKind::APIToken;
    result.sourceLabel = "api";

    if (json.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty or invalid response from Crof API";
        return result;
    }

    double credits = json["credits"].toDouble(0.0);
    double requestsPlan = json["requestsPlan"].toDouble(0.0);
    double usableRequests = json["usableRequests"].toDouble(0.0);

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::crof;
    snap.identity = identity;

    // 主窗口：请求使用率
    if (requestsPlan > 0) {
        RateWindow primary;
        double usedRequests = requestsPlan - usableRequests;
        primary.usedPercent = (usedRequests / requestsPlan) * 100.0;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 / %2 requests remaining")
            .arg(static_cast<int>(usableRequests))
            .arg(static_cast<int>(requestsPlan));
        snap.primary = primary;
    }

    // 次窗口：额度
    if (credits > 0) {
        RateWindow secondary;
        secondary.usedPercent = 0;
        secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 credits")
            .arg(credits, 0, 'f', 2);
        snap.secondary = secondary;
    }

    result.usage = snap;
    result.success = true;

    return result;
}
