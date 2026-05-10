#include "ChartDataProvider.h"

#include <QDateTime>
#include <QMap>
#include <cmath>
#include <algorithm>

// --- Cost History ---

QVariantList ChartDataProvider::buildCostHistory(
    const QVector<ProviderCostUsageSnapshot>& allProviders,
    const QString& providerId)
{
    QVariantList result;

    // Find the matching provider
    const ProviderCostUsageSnapshot* target = nullptr;
    for (const auto& p : allProviders) {
        if (p.providerId == providerId) {
            target = &p;
            break;
        }
    }
    if (!target) return result;

    for (const auto& entry : target->snapshot.daily) {
        QVariantMap point;
        point["date"] = entry.date;
        point["costUSD"] = entry.costUSD;

        QVariantList models;
        // Sort models by cost descending, take top 4
        auto sortedModels = entry.models;
        std::sort(sortedModels.begin(), sortedModels.end(),
            [](const CostUsageModelBreakdown& a, const CostUsageModelBreakdown& b) {
                return a.costUSD > b.costUSD;
            });

        int count = 0;
        for (const auto& m : sortedModels) {
            if (count >= 4) break;
            QVariantMap mm;
            mm["name"] = m.modelName;
            mm["costUSD"] = m.costUSD;
            mm["tokens"] = m.totalTokens();
            models.append(mm);
            ++count;
        }
        point["models"] = models;
        result.append(point);
    }

    // Find peak index
    int peakIdx = findPeakIndex(result, "costUSD");
    if (peakIdx >= 0 && peakIdx < result.size()) {
        result[peakIdx].toMap()["isPeak"] = true;
    }

    return result;
}

// --- Credits History ---

QVariantList ChartDataProvider::buildCreditsHistory(
    const std::optional<CreditsSnapshot>& credits,
    const QVector<CostUsageDailyEntry>& dailyEntries)
{
    QVariantList result;
    if (!credits.has_value()) return result;

    // Aggregate credits events by date
    QMap<QString, double> dailyCredits;
    QMap<QString, QStringList> dailyServices;

    for (const auto& event : credits->events) {
        QString date = event.timestamp.date().toString("yyyy-MM-dd");
        dailyCredits[date] += event.amount;
        if (!event.type.isEmpty() && !dailyServices[date].contains(event.type)) {
            dailyServices[date].append(event.type);
        }
    }

    // Align with daily entries (which have the full date range)
    for (const auto& entry : dailyEntries) {
        QVariantMap point;
        point["date"] = entry.date;
        point["creditsUsed"] = dailyCredits.value(entry.date, 0.0);

        QStringList services = dailyServices.value(entry.date);
        // Take top 3
        if (services.size() > 3) services = services.mid(0, 3);
        point["services"] = QVariant::fromValue(services);

        result.append(point);
    }

    // Find peak
    int peakIdx = findPeakIndex(result, "creditsUsed");
    if (peakIdx >= 0 && peakIdx < result.size()) {
        result[peakIdx].toMap()["isPeak"] = true;
    }

    return result;
}

// --- Usage Breakdown ---

QVariantList ChartDataProvider::buildUsageBreakdown(const QVariantMap& dashboardData)
{
    QVariantList result;
    if (dashboardData.isEmpty()) return result;

    // Extract dailyBreakdown from dashboard data
    const QVariantList dailyBreakdown = dashboardData.value("dailyBreakdown").toList();
    if (dailyBreakdown.isEmpty()) return result;

    auto colors = serviceColorMap();

    for (const QVariant& day : dailyBreakdown) {
        const QVariantMap dayMap = day.toMap();
        BreakdownPoint bp;
        bp.date = dayMap.value("date").toString();

        const QVariantList services = dayMap.value("services").toList();
        for (const QVariant& sv : services) {
            const QVariantMap svMap = sv.toMap();
            BreakdownService bs;
            bs.name = svMap.value("name").toString();
            bs.credits = svMap.value("credits").toDouble();
            bs.color = colors.value(bs.name, hashColor(bs.name));
            bp.services.append(bs);
            bp.totalCredits += bs.credits;
        }

        // Sort services by credits descending
        std::sort(bp.services.begin(), bp.services.end(),
            [](const BreakdownService& a, const BreakdownService& b) {
                return a.credits > b.credits;
            });

        // Top 6, rest → "Other"
        if (bp.services.size() > 6) {
            double otherCredits = 0;
            for (int i = 6; i < bp.services.size(); ++i) {
                otherCredits += bp.services[i].credits;
            }
            bp.services.resize(6);
            if (otherCredits > 0) {
                BreakdownService other;
                other.name = QStringLiteral("Other");
                other.credits = otherCredits;
                other.color = QStringLiteral("#9E9E9E");
                bp.services.append(other);
            }
        }

        QVariantMap point;
        point["date"] = bp.date;
        point["totalCredits"] = bp.totalCredits;
        QVariantList svList;
        for (const auto& s : bp.services) {
            QVariantMap sm;
            sm["name"] = s.name;
            sm["credits"] = s.credits;
            sm["color"] = s.color;
            svList.append(sm);
        }
        point["services"] = svList;
        result.append(point);
    }

    return result;
}

// --- Storage Breakdown ---

QVariantList ChartDataProvider::buildStorageBreakdown(
    const QVector<StorageComponent>& components,
    int maxVisible)
{
    QVariantList result;
    if (components.isEmpty()) return result;

    // Sort by bytes descending
    auto sorted = components;
    std::sort(sorted.begin(), sorted.end(),
        [](const StorageComponent& a, const StorageComponent& b) {
            return a.bytes > b.bytes;
        });

    qint64 maxBytes = sorted.first().bytes;

    int count = 0;
    for (const auto& comp : sorted) {
        if (count >= maxVisible) break;
        QVariantMap item;
        item["path"] = comp.path;
        item["bytes"] = comp.bytes;
        item["bytesDisplay"] = formatBytes(comp.bytes);
        item["fraction"] = maxBytes > 0 ? (double)comp.bytes / maxBytes : 0.0;
        item["canCopy"] = comp.canCopy;
        result.append(item);
        ++count;
    }

    if (sorted.size() > maxVisible) {
        QVariantMap more;
        more["isMoreIndicator"] = true;
        more["remainingCount"] = sorted.size() - maxVisible;
        result.append(more);
    }

    return result;
}

QVariantList ChartDataProvider::buildCleanupItems(const QVector<StorageCleanupItem>& items)
{
    QVariantList result;
    for (const auto& item : items) {
        QVariantMap m;
        m["title"] = item.title;
        m["path"] = item.path;
        m["bytes"] = item.bytes;
        m["bytesDisplay"] = formatBytes(item.bytes);
        m["consequence"] = item.consequence;
        result.append(m);
    }
    return result;
}

// --- Helpers ---

int ChartDataProvider::findPeakIndex(const QVariantList& points, const QString& valueKey)
{
    if (points.isEmpty()) return -1;
    int peakIdx = 0;
    double peakVal = points.first().toMap().value(valueKey).toDouble();
    for (int i = 1; i < points.size(); ++i) {
        double v = points[i].toMap().value(valueKey).toDouble();
        if (v > peakVal) {
            peakVal = v;
            peakIdx = i;
        }
    }
    return peakIdx;
}

QString ChartDataProvider::formatDateShort(const QString& isoDate)
{
    QDate d = QDate::fromString(isoDate, "yyyy-MM-dd");
    if (!d.isValid()) return isoDate;
    return d.toString("MMM d");
}

QString ChartDataProvider::formatBytes(qint64 bytes)
{
    if (bytes >= 1073741824)
        return QString::number(bytes / 1073741824.0, 'f', 1) + " GB";
    if (bytes >= 1048576)
        return QString::number(bytes / 1048576.0, 'f', 1) + " MB";
    if (bytes >= 1024)
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    return QString::number(bytes) + " B";
}

QHash<QString, QString> ChartDataProvider::serviceColorMap()
{
    return {
        {"CLI", "#4260F0"},
        {"GitHub Review", "#F0882E"},
        {"API", "#4CAF50"},
        {"Codex", "#9C27B0"},
        {"Codex CLI", "#E91E63"},
        {"Dashboard", "#00BCD4"},
        {"Storage", "#795548"},
    };
}

QString ChartDataProvider::hashColor(const QString& serviceName)
{
    int h = 0;
    for (const QChar& c : serviceName) {
        h = ((h << 5) - h) + c.unicode();
    }
    const QStringList palette = {"#FF9800", "#607D8B", "#CDDC39", "#3F51B5"};
    return palette[std::abs(h) % palette.size()];
}
