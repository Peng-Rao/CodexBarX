#include "StepFunProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

StepFunProvider::StepFunProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> StepFunProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new StepFunWebStrategy(this) };
}

StepFunWebStrategy::StepFunWebStrategy(QObject* parent) : IFetchStrategy(parent) {}

QString StepFunWebStrategy::resolveToken(const ProviderFetchContext& ctx) {
    // 1. 环境变量
    if (ctx.env.contains("STEPFUN_TOKEN")) return ctx.env["STEPFUN_TOKEN"];
    if (ctx.env.contains("OASIS_TOKEN")) return ctx.env["OASIS_TOKEN"];

    // 2. 设置中的 token
    auto settingsToken = ctx.settings.get("token");
    if (settingsToken.isValid() && !settingsToken.toString().isEmpty()) {
        return settingsToken.toString();
    }

    // 3. 凭证存储
    auto cred = ProviderCredentialStore::read("com.codexbarx.token.stepfun");
    if (cred.has_value()) return QString::fromUtf8(cred.value());

    return {};
}

bool StepFunWebStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveToken(ctx).isEmpty();
}

bool StepFunWebStrategy::shouldFallback(const ProviderFetchResult& result,
                                         const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult StepFunWebStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "web";

    QString token = resolveToken(ctx);
    if (token.isEmpty()) {
        result.success = false;
        result.errorMessage = "StepFun token not configured. Please login or enter token manually.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Cookie"] = "Oasis-Token=" + token;
    headers["Accept"] = "application/json";
    headers["Content-Type"] = "application/json";

    // 获取配额限制
    QJsonObject body;
    auto [json, httpStatus] = NetworkManager::instance().postJsonSyncWithStatus(
        QUrl("https://platform.stepfun.com/api/step.openapi.devcenter.Dashboard/QueryStepPlanRateLimit"),
        body, headers, ctx.networkTimeoutMs);

    result.httpStatus = httpStatus;

    if (json.isEmpty() && httpStatus != 200) {
        result.success = false;
        result.errorMessage = httpStatus == 401 ? "Token expired. Please re-login."
            : QString("HTTP error: %1").arg(httpStatus);
        return result;
    }

    return parseResponse(json);
}

ProviderFetchResult StepFunWebStrategy::parseResponse(const QJsonObject& json) {
    ProviderFetchResult result;
    result.strategyID = "stepfun.web";
    result.strategyKind = ProviderFetchKind::Web;
    result.sourceLabel = "web";

    if (json.isEmpty()) {
        result.success = false;
        result.errorMessage = "Empty response from StepFun API";
        return result;
    }

    double fiveHourRate = json["fiveHourUsageLeftRate"].toDouble(1.0);
    double weeklyRate = json["weeklyUsageLeftRate"].toDouble(1.0);
    qint64 fiveHourReset = json["fiveHourUsageResetTime"].toVariant().toLongLong();
    qint64 weeklyReset = json["weeklyUsageResetTime"].toVariant().toLongLong();

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::stepfun;
    snap.identity = identity;

    // 主窗口：5 小时配额
    {
        RateWindow primary;
        primary.usedPercent = (1.0 - fiveHourRate) * 100.0;
        primary.windowMinutes = 300;
        if (fiveHourReset > 0) {
            primary.resetsAt = QDateTime::fromSecsSinceEpoch(fiveHourReset);
        }
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1% remaining (5h window)")
            .arg(fiveHourRate * 100, 0, 'f', 0);
        snap.primary = primary;
    }

    // 次窗口：周配额
    {
        RateWindow secondary;
        secondary.usedPercent = (1.0 - weeklyRate) * 100.0;
        secondary.windowMinutes = 10080;
        if (weeklyReset > 0) {
            secondary.resetsAt = QDateTime::fromSecsSinceEpoch(weeklyReset);
        }
        secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1% remaining (weekly)")
            .arg(weeklyRate * 100, 0, 'f', 0);
        snap.secondary = secondary;
    }

    result.usage = snap;
    result.success = true;

    return result;
}
