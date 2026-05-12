#include "ClaudeWebExtraFetcher.h"
#include "../../network/NetworkManager.h"
#include "../../models/ClaudeUsageSnapshot.h"
#include "../../models/UsageSnapshot.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

ClaudeWebExtraFetcher& ClaudeWebExtraFetcher::instance()
{
    static ClaudeWebExtraFetcher inst;
    return inst;
}

ClaudeWebExtraData ClaudeWebExtraFetcher::fetch(
    const QString& sessionKey,
    const QString& organizationId,
    int timeoutMs)
{
    ClaudeWebExtraData result;

    if (sessionKey.isEmpty() || organizationId.isEmpty()) {
        return result;
    }

    QString jsonStr = fetchUsageJson(organizationId, sessionKey, timeoutMs);
    if (jsonStr.isEmpty()) {
        return result;
    }

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return result;
    }

    QJsonObject json = doc.object();

    // Parse extra rate windows using existing ClaudeUsageSnapshot logic
    ClaudeUsageSnapshot snap = ClaudeUsageSnapshot::fromWebJson(json);

    // Extract extra rate windows
    UsageSnapshot usage = snap.toUsageSnapshot();
    result.extraRateWindows = usage.extraRateWindows;

    // Parse provider cost (overage)
    if (json.contains("overage_spend_limit")) {
        QJsonObject overage = json.value("overage_spend_limit").toObject();
        if (overage.value("is_enabled").toBool(false)) {
            double usedCredits = overage.value("used_credits").toDouble(0);
            double monthlyLimit = overage.value("monthly_credit_limit").toDouble(0);
            QString currency = overage.value("currency").toString("USD");

            if (usedCredits > 0 || monthlyLimit > 0) {
                ProviderCostSnapshot cost;
                cost.used = usedCredits / 100.0;
                cost.limit = monthlyLimit / 100.0;
                cost.currencyCode = currency;
                cost.period = "Monthly";
                cost.updatedAt = QDateTime::currentDateTime();
                result.providerCost = cost;
            }
        }
    }

    return result;
}

QString ClaudeWebExtraFetcher::fetchUsageJson(
    const QString& orgId,
    const QString& sessionKey,
    int timeoutMs)
{
    QHash<QString, QString> headers;
    headers["Cookie"] = "sessionKey=" + sessionKey;
    headers["Accept"] = "application/json";

    return NetworkManager::instance().getStringSync(
        QUrl("https://claude.ai/api/organizations/" + orgId + "/usage"), headers, timeoutMs);
}
