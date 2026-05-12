#pragma once

#include "../../models/RateWindow.h"
#include "../../models/ProviderCostSnapshot.h"
#include <QString>
#include <QVector>
#include <optional>

struct ClaudeWebExtraData {
    QVector<NamedRateWindow> extraRateWindows;
    std::optional<ProviderCostSnapshot> providerCost;
};

class ClaudeWebExtraFetcher {
public:
    static ClaudeWebExtraFetcher& instance();

    ClaudeWebExtraData fetch(
        const QString& sessionKey,
        const QString& organizationId,
        int timeoutMs = 10000);

private:
    ClaudeWebExtraFetcher() = default;
    ~ClaudeWebExtraFetcher() = default;

    QString fetchUsageJson(
        const QString& orgId,
        const QString& sessionKey,
        int timeoutMs);
};
