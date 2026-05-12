#include "DoubaoProvider.h"
#include "../../network/NetworkManager.h"
#include "../../providers/shared/ProviderCredentialStore.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

DoubaoProvider::DoubaoProvider(QObject* parent) : IProvider(parent) {}

QVector<IFetchStrategy*> DoubaoProvider::createStrategies(const ProviderFetchContext& ctx) {
    Q_UNUSED(ctx)
    return { new DoubaoAPIStrategy(this) };
}

DoubaoAPIStrategy::DoubaoAPIStrategy(QObject* parent) : IFetchStrategy(parent) {}

const char* DoubaoAPIStrategy::PROBE_MODELS[] = {
    "doubao-seed-1.6",
    "doubao-1.5-pro-32k",
    "doubao-lite-32k"
};

QString DoubaoAPIStrategy::resolveApiKey(const ProviderFetchContext& ctx) {
    if (ctx.env.contains("ARK_API_KEY")) return ctx.env["ARK_API_KEY"];
    if (ctx.env.contains("DOUBAO_API_KEY")) return ctx.env["DOUBAO_API_KEY"];
    if (ctx.accountCredentials.api.has_value() && ctx.accountCredentials.api->isValid()) {
        return ctx.accountCredentials.api->apiKey.toString().trimmed();
    }
    auto cred = ProviderCredentialStore::read("com.codexbarx.apikey.doubao");
    if (cred.has_value()) return QString::fromUtf8(cred.value());
    return {};
}

bool DoubaoAPIStrategy::isAvailable(const ProviderFetchContext& ctx) const {
    return !resolveApiKey(ctx).isEmpty();
}

bool DoubaoAPIStrategy::shouldFallback(const ProviderFetchResult& result,
                                        const ProviderFetchContext& ctx) const {
    Q_UNUSED(ctx)
    return !result.success;
}

ProviderFetchResult DoubaoAPIStrategy::fetchSync(const ProviderFetchContext& ctx) {
    ProviderFetchResult result;
    result.strategyID = id();
    result.strategyKind = kind();
    result.sourceLabel = "api";

    QString apiKey = resolveApiKey(ctx);
    if (apiKey.isEmpty()) {
        result.success = false;
        result.errorMessage = "Doubao API key not configured.";
        return result;
    }

    QHash<QString, QString> headers;
    headers["Authorization"] = "Bearer " + apiKey;
    headers["Accept"] = "application/json";

    // 尝试多个模型进行探测
    for (const char* model : PROBE_MODELS) {
        QJsonObject body;
        body["model"] = QString::fromUtf8(model);
        body["messages"] = QJsonArray();
        body["max_tokens"] = 1;

        auto [json, httpStatus, respHeaders] = NetworkManager::instance().postJsonSyncWithHeaders(
            QUrl("https://ark.cn-beijing.volces.com/api/v3/chat/completions"),
            body, headers, ctx.networkTimeoutMs);

        // 即使请求失败，也检查响应头中的 rate limit 信息
        if (respHeaders.contains("x-ratelimit-remaining-requests") ||
            respHeaders.contains("X-RateLimit-Remaining-Requests")) {
            return parseFromHeaders(respHeaders, httpStatus);
        }

        // 如果是认证错误，直接返回
        if (httpStatus == 401 || httpStatus == 403) {
            result.success = false;
            result.errorMessage = "Invalid API key or unauthorized access.";
            result.httpStatus = httpStatus;
            return result;
        }
    }

    result.success = false;
    result.errorMessage = "No rate limit headers found in response.";
    return result;
}

ProviderFetchResult DoubaoAPIStrategy::parseFromHeaders(const QHash<QString, QString>& headers, int httpStatus) {
    ProviderFetchResult result;
    result.strategyID = "doubao.api";
    result.strategyKind = ProviderFetchKind::APIToken;
    result.sourceLabel = "api";
    result.httpStatus = httpStatus;

    // Headers 可能是大小写不敏感的，检查两种格式
    auto getHeader = [&headers](const QString& key) -> QString {
        if (headers.contains(key)) return headers[key];
        QString lowerKey = key.toLower();
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) {
            if (it.key().toLower() == lowerKey) return it.value();
        }
        return {};
    };

    QString remainingStr = getHeader("x-ratelimit-remaining-requests");
    QString limitStr = getHeader("x-ratelimit-limit-requests");
    QString resetStr = getHeader("x-ratelimit-reset-requests");

    int remaining = remainingStr.toInt();
    int limit = limitStr.toInt();

    UsageSnapshot snap;
    snap.updatedAt = QDateTime::currentDateTime();

    ProviderIdentitySnapshot identity;
    identity.providerID = UsageProvider::doubao;
    snap.identity = identity;

    if (limit > 0) {
        RateWindow primary;
        int used = limit - remaining;
        primary.usedPercent = (used * 100.0) / limit;
        primary.resetDescription = QCoreApplication::translate("ProviderLabels", "%1 / %2 requests remaining")
            .arg(remaining)
            .arg(limit);
        snap.primary = primary;
    }

    // 解析重置时间
    if (!resetStr.isEmpty()) {
        // 可能是 ISO 格式或秒数
        bool ok = false;
        int resetSeconds = resetStr.toInt(&ok);
        if (ok && resetSeconds > 0) {
            RateWindow secondary;
            secondary.usedPercent = 0;
            secondary.windowMinutes = resetSeconds / 60;
            secondary.resetsAt = QDateTime::currentDateTime().addSecs(resetSeconds);
            secondary.resetDescription = QCoreApplication::translate("ProviderLabels", "Resets in %1 min")
                .arg(resetSeconds / 60);
            snap.secondary = secondary;
        }
    }

    result.usage = snap;
    result.success = (limit > 0);

    return result;
}
