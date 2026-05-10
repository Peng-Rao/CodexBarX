#pragma once

#include "models/CostUsageReport.h"
#include "models/CreditsSnapshot.h"

#include <QHash>
#include <QString>
#include <QVariantList>
#include <optional>

struct ChartPoint {
    QString date;
    double value = 0.0;
    bool isPeak = false;
};

struct CostHistoryPoint {
    QString date;
    double costUSD = 0.0;
    bool isPeak = false;
    QVariantList models; // [{ name, costUSD, tokens }]
};

struct CreditsHistoryPoint {
    QString date;
    double creditsUsed = 0.0;
    bool isPeak = false;
    QStringList services; // top service names
};

struct BreakdownService {
    QString name;
    double credits = 0.0;
    QString color;
};

struct BreakdownPoint {
    QString date;
    double totalCredits = 0.0;
    QVector<BreakdownService> services;
};

struct StorageComponent {
    QString path;
    qint64 bytes = 0;
    double fraction = 0.0;
    bool canCopy = true;
};

struct StorageCleanupItem {
    QString title;
    QString path;
    qint64 bytes = 0;
    QString consequence;
};

class ChartDataProvider {
public:
    // Cost history: per-provider daily cost bars
    static QVariantList buildCostHistory(
        const QVector<ProviderCostUsageSnapshot>& allProviders,
        const QString& providerId);

    // Credits history: Codex daily credits used
    static QVariantList buildCreditsHistory(
        const std::optional<CreditsSnapshot>& credits,
        const QVector<CostUsageDailyEntry>& dailyEntries);

    // Usage breakdown: per-service stacked bars
    static QVariantList buildUsageBreakdown(
        const QVariantMap& dashboardData);

    // Storage breakdown: file system components
    static QVariantList buildStorageBreakdown(
        const QVector<StorageComponent>& components,
        int maxVisible = 8);

    // Storage cleanup items
    static QVariantList buildCleanupItems(
        const QVector<StorageCleanupItem>& items);

private:
    static int findPeakIndex(const QVariantList& points, const QString& valueKey);
    static QString formatDateShort(const QString& isoDate);
    static QString formatBytes(qint64 bytes);
    static QHash<QString, QString> serviceColorMap();
    static QString hashColor(const QString& serviceName);
};
