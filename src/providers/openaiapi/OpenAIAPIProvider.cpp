#include "OpenAIAPIProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

OpenAIAPIProvider::OpenAIAPIProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> OpenAIAPIProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new OpenAIAPIAPIStrategy(this) };
}

OpenAIAPIAPIStrategy::OpenAIAPIAPIStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString OpenAIAPIAPIStrategy::resolveApiKey(const ProviderFetchContext& ctx) {
    if (ctx.env.contains("OPENAI_API_KEY")) return ctx.env["OPENAI_API_KEY"];
    if (ctx.accountCredentials.api.has_value() && ctx.accountCredentials.api->isValid()) {
        return ctx.accountCredentials.api->apiKey.toString().trimmed();
    }
    auto cred = ProviderCredentialStore::read("com.codexbarx.apikey.openaiapi");
    if (cred.has_value()) return QString::fromUtf8(cred.value());
    return {};
}

bool OpenAIAPIAPIStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveApiKey(ctx).isEmpty();
}

bool OpenAIAPIAPIStrategy::shouldFallback(const ProviderFetchResult& result,
                                            const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult OpenAIAPIAPIStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "api";

    QString apiKey = resolveApiKey(ctx);
    if (apiKey.isEmpty()) {
        result.success = false;
        result.errorMessage = "OpenAI API key not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Authorization"] = "Bearer " + apiKey;
    headers["Accept"] = "application/json";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://api.openai.com/v1/dashboard/billing/credit_grants"), headers, ctx.networkTimeoutMs);

    // 检查是否有错误响应
    if (json.contains("error")) {
        QJsonObject error = json["error"].toObject();
        QString errorMsg = error["message"].toString();
        if (errorMsg.contains("permission", Qt::CaseInsensitive) ||
            errorMsg.contains("403", Qt::CaseInsensitive)) {
            result.success = false;
            result.errorMessage = QCoreApplication::translate("ProviderLabels",
                "Project keys may not expose credit grants. Use a legacy/user API key.");
            result.httpStatus = 403;
            return result;
        }
    }

    return parseResponse(json);
}

ProviderFetchResult OpenAIAPIAPIStrategy::parseResponse(const QJsonObject& json, int httpStatus) {
    ProviderFetchResult result;
    result.strategyID = "openaiapi.api";
    result.strategyKind = ProviderFetchKind::APIToken;
    result.sourceLabel = "api";
    result.httpStatus = httpStatus;

    if (json.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty or invalid response from OpenAI API";
        return result;
    }

    // 检查错误
    if (json.contains("error")) {
        result.success = false;
        result.errorMessage = json["error"].toObject()["message"].toString();
        return result;
    }

    double totalGranted = json["total_granted"].toDouble(0.0);
    double totalUsed = json["total_used"].toDouble(0.0);
    double totalAvailable = json["total_available"].toDouble(0.0);

    QJsonArray grants = json["credit_grants"].toArray();

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::openaiapi;
    snap.identity = identity;

    // 主窗口：可用额度
    RateWindow primary;
    if (totalGranted > 0) {
        primary.usedPercent = (totalUsed / totalGranted) * 100.0;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "$%1 / $%2 available")
            .arg(totalAvailable, 0, 'f', 2)
            .arg(totalGranted, 0, 'f', 2);
    } else if (totalAvailable > 0) {
        primary.usedPercent = 0;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "$%1 available")
            .arg(totalAvailable, 0, 'f', 2);
    } else {
        primary.usedPercent = 100;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "No credits available");
    }
    snap.primary = primary;

    // 查找最近的过期时间
    QDateTime nextExpiry;
    for (const auto& grantValue : grants) {
        QJsonObject grant = grantValue.toObject();
        double grantAmount = grant["grant_amount"].toDouble(0.0);
        double usedAmount = grant["used_amount"].toDouble(0.0);
        if (grantAmount > usedAmount) {
            QString expiresAtStr = grant["expires_at"].toString();
            if (!expiresAtStr.isEmpty()) {
                QDateTime expiry = QDateTime::fromString(expiresAtStr, Qt::ISODate);
                if (expiry.isValid() && (!nextExpiry.isValid() || expiry < nextExpiry)) {
                    nextExpiry = expiry;
                }
            }
        }
    }

    // 次窗口：过期信息
    if (nextExpiry.isValid()) {
        RateWindow secondary;
        secondary.usedPercent = 0;
        secondary.resetsAt = nextExpiry;
        secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "Expires %1")
            .arg(nextExpiry.toString("yyyy-MM-dd"));
        snap.secondary = secondary;
    }

    result.usage = snap;
    result.success = true;

    return result;
}
