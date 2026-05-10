#include "app/ChartDataProvider.h"
#include <QtTest/QtTest>

class tst_ChartDataProvider : public QObject {
    Q_OBJECT
private slots:
    void t1_costHistory_empty();
    void t2_costHistory_basic();
    void t3_creditsHistory_empty();
    void t4_storageBreakdown_sorting();
    void t5_storageBreakdown_truncation();
    void t6_usageBreakdown_empty();
    void t7_usageBreakdown_basic();
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

void tst_ChartDataProvider::t3_creditsHistory_empty()
{ QVERIFY(ChartDataProvider::buildCreditsHistory(std::nullopt, {}).isEmpty()); }

void tst_ChartDataProvider::t4_storageBreakdown_sorting()
{
    QVector<StorageComponent> comps;
    StorageComponent a; a.path = "/a"; a.bytes = 10; comps.append(a);
    StorageComponent b; b.path = "/b"; b.bytes = 100; comps.append(b);
    QVariantList r = ChartDataProvider::buildStorageBreakdown(comps);
    QCOMPARE(r.size(), 2);
    QCOMPARE(r[0].toMap()["path"].toString(), QString("/b"));
}

void tst_ChartDataProvider::t5_storageBreakdown_truncation()
{
    QVector<StorageComponent> comps;
    for (int i = 0; i < 15; ++i) {
        StorageComponent sc; sc.path = "p" + QString::number(i); sc.bytes = (15-i)*100; comps.append(sc);
    }
    QVariantList r = ChartDataProvider::buildStorageBreakdown(comps, 8);
    QCOMPARE(r.size(), 9);
}

void tst_ChartDataProvider::t6_usageBreakdown_empty()
{ QVERIFY(ChartDataProvider::buildUsageBreakdown(QVariantMap()).isEmpty()); }

void tst_ChartDataProvider::t7_usageBreakdown_basic()
{
    QVariantMap sv; sv["name"] = "CLI"; sv["credits"] = 5.0;
    QVariantList svList; svList.append(sv);
    QVariantMap day; day["date"] = "x"; day["services"] = svList;
    QVariantList days; days.append(day);
    QVariantMap dash; dash["dailyBreakdown"] = days;
    QCOMPARE(ChartDataProvider::buildUsageBreakdown(dash).size(), 1);
}

QTEST_MAIN(tst_ChartDataProvider)
#include "tst_ChartDataProvider.moc"
