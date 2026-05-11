#include "app/ChartDataProvider.h"
#include "models/CreditsSnapshot.h"
#include <QtTest/QtTest>
#include <QDateTime>

class tst_ChartDataProvider : public QObject {
    Q_OBJECT
private slots:
    void t1_costHistory_empty();
    void t2_costHistory_basic();
    void t3_costHistory_peakIndex();
    void t4_costHistory_modelBreakdown();
    void t5_creditsHistory_empty();
    void t6_creditsHistory_dailyAggregation();
    void t7_storageBreakdown_sorting();
    void t8_storageBreakdown_truncation();
    void t9_usageBreakdown_empty();
    void t10_usageBreakdown_top6();
};

void tst_ChartDataProvider::t1_costHistory_empty()
{ QVector<ProviderCostUsageSnapshot> all; QVERIFY(ChartDataProvider::buildCostHistory(all, "codex").isEmpty()); }

void tst_ChartDataProvider::t2_costHistory_basic()
{
    ProviderCostUsageSnapshot snap; snap.providerId = "codex";
    CostUsageDailyEntry day; day.date = "d"; day.costUSD = 1.0;
    snap.snapshot.daily.append(day);
    QVector<ProviderCostUsageSnapshot> all; all.append(snap);
    QCOMPARE(ChartDataProvider::buildCostHistory(all, "codex").size(), 1);
}

void tst_ChartDataProvider::t3_costHistory_peakIndex()
{
    // 3 days with costs 1.0, 5.0, 2.0 - peak should be at index 1
    ProviderCostUsageSnapshot snap; snap.providerId = "codex";
    CostUsageDailyEntry d1; d1.date = "2026-05-08"; d1.costUSD = 1.0; snap.snapshot.daily.append(d1);
    CostUsageDailyEntry d2; d2.date = "2026-05-09"; d2.costUSD = 5.0; snap.snapshot.daily.append(d2);
    CostUsageDailyEntry d3; d3.date = "2026-05-10"; d3.costUSD = 2.0; snap.snapshot.daily.append(d3);
    QVector<ProviderCostUsageSnapshot> all; all.append(snap);
    QVariantList result = ChartDataProvider::buildCostHistory(all, "codex");
    QCOMPARE(result.size(), 3);
    // Peak index 1 should have isPeak = true
    QVERIFY(result[1].toMap().value("isPeak").toBool());
    QVERIFY(!result[0].toMap().contains("isPeak") || !result[0].toMap().value("isPeak").toBool());
    QVERIFY(!result[2].toMap().contains("isPeak") || !result[2].toMap().value("isPeak").toBool());
}

void tst_ChartDataProvider::t4_costHistory_modelBreakdown()
{
    ProviderCostUsageSnapshot snap; snap.providerId = "codex";
    CostUsageDailyEntry day; day.date = "2026-05-10"; day.costUSD = 1.5;
    CostUsageModelBreakdown m1; m1.modelName = "gpt-5"; m1.costUSD = 1.0; m1.inputTokens = 1000; m1.outputTokens = 500;
    CostUsageModelBreakdown m2; m2.modelName = "claude-opus"; m2.costUSD = 0.5; m2.inputTokens = 500; m2.outputTokens = 200;
    day.models.append(m1);
    day.models.append(m2);
    snap.snapshot.daily.append(day);
    QVector<ProviderCostUsageSnapshot> all; all.append(snap);
    QVariantList result = ChartDataProvider::buildCostHistory(all, "codex");
    QCOMPARE(result.size(), 1);
    QVariantList models = result[0].toMap().value("models").toList();
    QCOMPARE(models.size(), 2);
    QCOMPARE(models[0].toMap().value("name").toString(), QString("gpt-5"));
    QCOMPARE(models[0].toMap().value("costUSD").toDouble(), 1.0);
    QCOMPARE(models[1].toMap().value("name").toString(), QString("claude-opus"));
}

void tst_ChartDataProvider::t5_creditsHistory_empty()
{ QVERIFY(ChartDataProvider::buildCreditsHistory(std::nullopt, {}).isEmpty()); }

void tst_ChartDataProvider::t6_creditsHistory_dailyAggregation()
{
    // Multiple credits events on the same day should be aggregated
    CreditsSnapshot credits;
    CreditEvent e1; e1.timestamp = QDateTime::fromString("2026-05-10T10:00:00", Qt::ISODate); e1.amount = 2.5; e1.type = "CLI";
    CreditEvent e2; e2.timestamp = QDateTime::fromString("2026-05-10T14:00:00", Qt::ISODate); e2.amount = 1.5; e2.type = "API";
    CreditEvent e3; e3.timestamp = QDateTime::fromString("2026-05-11T10:00:00", Qt::ISODate); e3.amount = 3.0; e3.type = "CLI";
    credits.events.append(e1);
    credits.events.append(e2);
    credits.events.append(e3);

    CostUsageDailyEntry d1; d1.date = "2026-05-10";
    CostUsageDailyEntry d2; d2.date = "2026-05-11";
    QVector<CostUsageDailyEntry> daily; daily.append(d1); daily.append(d2);

    QVariantList result = ChartDataProvider::buildCreditsHistory(credits, daily);
    QCOMPARE(result.size(), 2);
    // Day 1: 2.5 + 1.5 = 4.0
    QCOMPARE(result[0].toMap().value("creditsUsed").toDouble(), 4.0);
    // Day 2: 3.0
    QCOMPARE(result[1].toMap().value("creditsUsed").toDouble(), 3.0);
}

void tst_ChartDataProvider::t7_storageBreakdown_sorting()
{
    QVector<StorageComponent> comps;
    StorageComponent a; a.path = "/a"; a.bytes = 10; comps.append(a);
    StorageComponent b; b.path = "/b"; b.bytes = 100; comps.append(b);
    QVariantList r = ChartDataProvider::buildStorageBreakdown(comps);
    QCOMPARE(r.size(), 2);
    QCOMPARE(r[0].toMap()["path"].toString(), QString("/b"));
}

void tst_ChartDataProvider::t8_storageBreakdown_truncation()
{
    QVector<StorageComponent> comps;
    for (int i = 0; i < 15; ++i) {
        StorageComponent sc; sc.path = "p" + QString::number(i); sc.bytes = (15-i)*100; comps.append(sc);
    }
    QVariantList r = ChartDataProvider::buildStorageBreakdown(comps, 8);
    QCOMPARE(r.size(), 9);  // 8 items + 1 "more" indicator
}

void tst_ChartDataProvider::t9_usageBreakdown_empty()
{ QVERIFY(ChartDataProvider::buildUsageBreakdown(QVariantMap()).isEmpty()); }

void tst_ChartDataProvider::t10_usageBreakdown_top6()
{
    // 8 services - should return top 6 per day, rest merged into "Other"
    QVariantList svList;
    for (int i = 0; i < 8; ++i) {
        QVariantMap sv;
        sv["name"] = "Service" + QString::number(i);
        sv["credits"] = (8 - i) * 1.0;  // Service0=8, Service1=7, ..., Service7=1
        svList.append(sv);
    }
    QVariantMap day; day["date"] = "2026-05-10"; day["services"] = svList;
    QVariantList days; days.append(day);
    QVariantMap dash; dash["dailyBreakdown"] = days;

    QVariantList result = ChartDataProvider::buildUsageBreakdown(dash);
    QCOMPARE(result.size(), 1);
    QVariantList services = result[0].toMap().value("services").toList();
    // Should have 6 services + 1 "Other" = 7
    QCOMPARE(services.size(), 7);
    // First should be Service0 (highest credits)
    QCOMPARE(services[0].toMap().value("name").toString(), QString("Service0"));
}

QTEST_MAIN(tst_ChartDataProvider)
#include "tst_ChartDataProvider.moc"
