#pragma once

#include "UsageBackendTypes.h"

#include <QSet>
#include <QString>
#include <QVector>

struct CostUsageScanPlan {
    QSet<QString> enabledProviderIds;
    QSet<QString> subscribedProviderIds;
    QSet<QString> openCodeDBProviderIds;
    bool scanClaude = false;
    bool scanCodex = false;
    bool scanPi = false;
    bool scanOpenCodeDB = false;
    bool includeAllOpenCodeDBProviders = false;
    bool scanOpenCodeGo = false;

    bool hasWork() const;
};

class CostUsageService {
public:
    static CostUsageScanPlan buildScanPlan(const QVector<QString>& enabledProviderIds,
                                           const QSet<QString>& subscribedProviderIds);
    static CostUsageRefreshPayload refresh(const CostUsageScanPlan& plan);
    static QVariantMap summaryData(const CostUsageSnapshot& snapshot);
    static QVariantMap summaryDataForProvider(const QString& providerId,
                                              const CostUsageSnapshot& combined,
                                              const QVector<ProviderCostUsageSnapshot>& providers);
    static QVariantList providerRows(const QVector<ProviderCostUsageSnapshot>& providers);
    static CostUsageDetailsRowsPayload detailsRows(const QVector<ProviderCostUsageSnapshot>& tokenProviders,
                                                   const QVariantList& appProviders);
    static CostUsageProviderDetailPayload providerDetail(const QString& providerId,
                                                         const QVector<ProviderCostUsageSnapshot>& providers);
    static void setShuttingDown(bool shuttingDown);
};
