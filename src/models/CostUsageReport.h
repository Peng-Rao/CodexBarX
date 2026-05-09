#pragma once

#include <QString>
#include <QVector>
#include <QDateTime>
#include <QtGlobal>
#include <optional>

struct CostUsageModelBreakdown {
    QString modelName;
    qint64 inputTokens = 0;
    qint64 cacheReadTokens = 0;
    qint64 cacheCreationTokens = 0;
    qint64 outputTokens = 0;
    qint64 totalTokens() const { return inputTokens + cacheReadTokens + cacheCreationTokens + outputTokens; }
    double costUSD = 0.0;
};

struct CostUsageDailyEntry {
    QString date;
    qint64 inputTokens = 0;
    qint64 cacheReadTokens = 0;
    qint64 cacheCreationTokens = 0;
    qint64 outputTokens = 0;
    qint64 totalTokens() const { return inputTokens + cacheReadTokens + cacheCreationTokens + outputTokens; }
    double costUSD = 0.0;
    QVector<CostUsageModelBreakdown> models;
};

struct CostUsageSnapshot {
    qint64 sessionTokens = 0;
    double sessionCostUSD = 0.0;
    qint64 last30DaysTokens = 0;
    double last30DaysCostUSD = 0.0;
    QVector<CostUsageDailyEntry> daily;
    QDateTime updatedAt;
    QString errorMessage;
};

struct ProviderCostUsageSnapshot {
    QString providerId;
    CostUsageSnapshot snapshot;
    QVector<CostUsageModelBreakdown> modelSummary;
};
