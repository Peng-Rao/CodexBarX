#include "VeniceProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

VeniceProvider::VeniceProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> VeniceProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new VeniceAPIStrategy(this) };
}

VeniceAPIStrategy::VeniceAPIStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString VeniceAPIStrategy::resolveApiKey(const ProviderFetchContext& ctx) {
    if (ctx.env.contains("VENICE_API_KEY")) return ctx.env["VENICE_API_KEY"];
    if (ctx.accountCredentials.api.has_value() && ctx.accountCredentials.api->isValid()) {
        return ctx.accountCredentials.api->apiKey.toString().trimmed();
    }
    auto cred = ProviderCredentialStore::read("com.codexbarx.apikey.venice");
    if (cred.has_value()) return QString::fromUtf8(cred.value());
    return {};
}

bool VeniceAPIStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveApiKey(ctx).isEmpty();
}

bool VeniceAPIStrategy::shouldFallback(const ProviderFetchResult& result,
                                        const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult VeniceAPIStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "api";

    QString apiKey = resolveApiKey(ctx);
    if (apiKey.isEmpty()) {
        result.success = false;
        result.errorMessage = "Venice API key not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Authorization"] = "Bearer " + apiKey;
    headers["Accept"] = "application/json";

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl("https://api.venice.ai/api/v1/billing/balance"), headers, ctx.networkTimeoutMs);

    return parseResponse(json);
}

ProviderFetchResult VeniceAPIStrategy::parseResponse(const QJsonObject& json) {
    ProviderFetchResult result;
    result.strategyID = "venice.api";
    result.strategyKind = ProviderFetchKind::APIToken;
    result.sourceLabel = "api";

    if (json.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty or invalid response from Venice API";
        return result;
    }

    bool canConsume = json["canConsume"].toBool(false);
    QString consumptionCurrency = json["consumptionCurrency"].toString("USD");

    QJsonObject balances = json["balances"].toObject();
    double diemBalance = balances["diem"].toDouble(0.0);
    double usdBalance = balances["usd"].toDouble(0.0);
    double diemEpochAllocation = json["diemEpochAllocation"].toDouble(0.0);

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::venice;
    snap.identity = identity;

    // 根据消费货币选择主要显示
    if (consumptionCurrency == "DIEM" && diemBalance > 0) {
        RateWindow primary;
        primary.usedPercent = 0;
        if (diemEpochAllocation > 0) {
            primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 DIEM (epoch: %2)")
                .arg(diemBalance, 0, 'f', 2)
                .arg(diemEpochAllocation, 0, 'f', 2);
        } else {
            primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 DIEM")
                .arg(diemBalance, 0, 'f', 2);
        }
        snap.primary = primary;

        // 次窗口显示 USD 余额
        if (usdBalance > 0) {
            RateWindow secondary;
            secondary.usedPercent = 0;
            secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "$%1 USD")
                .arg(usdBalance, 0, 'f', 2);
            snap.secondary = secondary;
        }
    } else {
        // 默认显示 USD
        RateWindow primary;
        primary.usedPercent = (usdBalance <= 0 && !canConsume) ? 100 : 0;
        if (usdBalance > 0) {
            primary.resetDescription = QCoreApplication::translate("ProviderLabels", "$%1 USD")
                .arg(usdBalance, 0, 'f', 2);
        } else if (!canConsume) {
            primary.resetDescription = QCoreApplication::translate("ProviderLabels", "No balance available");
        }
        snap.primary = primary;

        // 次窗口显示 DIEM 余额
        if (diemBalance > 0) {
            RateWindow secondary;
            secondary.usedPercent = 0;
            secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 DIEM")
                .arg(diemBalance, 0, 'f', 2);
            snap.secondary = secondary;
        }
    }

    result.usage = snap;
    result.success = true;

    return result;
}
