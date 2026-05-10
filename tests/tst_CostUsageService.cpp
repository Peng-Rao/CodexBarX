#include "app/CostUsageService.h"

#include <QtTest/QtTest>

class CostUsageServiceTest : public QObject {
    Q_OBJECT

private slots:
    void scanPlanIgnoresNonTokenProviders();
    void scanPlanEnablesNativeTokenProviders();
    void scanPlanScopesOpenCodeDatabase();
    void scanPlanIncludesLocalUsageProvidersWithoutAccounts();
    void scanPlanStillFiltersAccountScopedProviders();
    void summaryDataScopesToSelectedProvider();
};

void CostUsageServiceTest::scanPlanIgnoresNonTokenProviders()
{
    const CostUsageScanPlan plan = CostUsageService::buildScanPlan(
        {
            QStringLiteral("zai"),
            QStringLiteral("copilot"),
        },
        {
            QStringLiteral("zai"),
            QStringLiteral("copilot"),
        });

    QVERIFY(!plan.hasWork());
    QVERIFY(!plan.scanClaude);
    QVERIFY(!plan.scanCodex);
    QVERIFY(!plan.scanPi);
    QVERIFY(!plan.scanOpenCodeDB);
    QVERIFY(!plan.scanOpenCodeGo);
}

void CostUsageServiceTest::scanPlanEnablesNativeTokenProviders()
{
    const CostUsageScanPlan plan = CostUsageService::buildScanPlan(
        {
            QStringLiteral("claude"),
            QStringLiteral("codex"),
        },
        {
            QStringLiteral("claude"),
            QStringLiteral("codex"),
        });

    QVERIFY(plan.hasWork());
    QVERIFY(plan.scanClaude);
    QVERIFY(plan.scanCodex);
    QVERIFY(plan.scanPi);
    QVERIFY(!plan.scanOpenCodeDB);
    QVERIFY(!plan.scanOpenCodeGo);
}

void CostUsageServiceTest::scanPlanScopesOpenCodeDatabase()
{
    const CostUsageScanPlan openCodeGoPlan = CostUsageService::buildScanPlan(
        {
            QStringLiteral("opencodego"),
        },
        {
            QStringLiteral("opencodego"),
        });
    QVERIFY(openCodeGoPlan.hasWork());
    QVERIFY(openCodeGoPlan.scanOpenCodeGo);
    QVERIFY(openCodeGoPlan.scanOpenCodeDB);
    QVERIFY(!openCodeGoPlan.includeAllOpenCodeDBProviders);
    QCOMPARE(openCodeGoPlan.openCodeDBProviderIds, QSet<QString>{QStringLiteral("opencodego")});

    const CostUsageScanPlan openCodePlan = CostUsageService::buildScanPlan(
        {
            QStringLiteral("opencode"),
        },
        {
            QStringLiteral("opencode"),
        });
    QVERIFY(openCodePlan.hasWork());
    QVERIFY(!openCodePlan.scanOpenCodeGo);
    QVERIFY(openCodePlan.scanOpenCodeDB);
    QVERIFY(openCodePlan.includeAllOpenCodeDBProviders);
    QVERIFY(openCodePlan.openCodeDBProviderIds.isEmpty());
}

void CostUsageServiceTest::scanPlanIncludesLocalUsageProvidersWithoutAccounts()
{
    const CostUsageScanPlan plan = CostUsageService::buildScanPlan(
        {
            QStringLiteral("claude"),
            QStringLiteral("codex"),
            QStringLiteral("opencode"),
            QStringLiteral("opencodego"),
        },
        {});

    QVERIFY(plan.hasWork());
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("claude")));
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("codex")));
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("opencode")));
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("opencodego")));
    QVERIFY(plan.scanClaude);
    QVERIFY(plan.scanCodex);
    QVERIFY(plan.scanPi);
    QVERIFY(plan.scanOpenCodeGo);
    QVERIFY(plan.scanOpenCodeDB);
    QVERIFY(plan.includeAllOpenCodeDBProviders);
}

void CostUsageServiceTest::scanPlanStillFiltersAccountScopedProviders()
{
    const CostUsageScanPlan plan = CostUsageService::buildScanPlan(
        {
            QStringLiteral("claude"),
            QStringLiteral("codex"),
            QStringLiteral("opencodego"),
            QStringLiteral("zai"),
        },
        {
            QStringLiteral("zai"),
        });

    QVERIFY(plan.hasWork());
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("claude")));
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("codex")));
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("opencodego")));
    QVERIFY(plan.enabledProviderIds.contains(QStringLiteral("zai")));
    QVERIFY(plan.scanClaude);
    QVERIFY(plan.scanCodex);
    QVERIFY(plan.scanPi);
    QVERIFY(plan.scanOpenCodeGo);
    QVERIFY(plan.scanOpenCodeDB);
}

void CostUsageServiceTest::summaryDataScopesToSelectedProvider()
{
    CostUsageSnapshot combined;
    combined.sessionTokens = 600;
    combined.sessionCostUSD = 0.60;
    combined.last30DaysTokens = 6000;
    combined.last30DaysCostUSD = 6.0;

    ProviderCostUsageSnapshot codex;
    codex.providerId = QStringLiteral("codex");
    codex.snapshot.sessionTokens = 100;
    codex.snapshot.sessionCostUSD = 0.10;
    codex.snapshot.last30DaysTokens = 1000;
    codex.snapshot.last30DaysCostUSD = 1.0;

    ProviderCostUsageSnapshot claude;
    claude.providerId = QStringLiteral("claude");
    claude.snapshot.sessionTokens = 500;
    claude.snapshot.sessionCostUSD = 0.50;
    claude.snapshot.last30DaysTokens = 5000;
    claude.snapshot.last30DaysCostUSD = 5.0;

    const QVector<ProviderCostUsageSnapshot> providers = {codex, claude};

    const QVariantMap overview = CostUsageService::summaryDataForProvider({}, combined, providers);
    QCOMPARE(overview.value(QStringLiteral("sessionTokens")).toLongLong(), 600);
    QCOMPARE(overview.value(QStringLiteral("last30DaysTokens")).toLongLong(), 6000);

    const QVariantMap selected = CostUsageService::summaryDataForProvider(
        QStringLiteral("codex"), combined, providers);
    QCOMPARE(selected.value(QStringLiteral("sessionTokens")).toLongLong(), 100);
    QCOMPARE(selected.value(QStringLiteral("last30DaysTokens")).toLongLong(), 1000);

    const QVariantMap missing = CostUsageService::summaryDataForProvider(
        QStringLiteral("missing"), combined, providers);
    QCOMPARE(missing.value(QStringLiteral("sessionTokens")).toLongLong(), 0);
    QCOMPARE(missing.value(QStringLiteral("last30DaysTokens")).toLongLong(), 0);
    QCOMPARE(missing.value(QStringLiteral("hasData")).toBool(), false);
}

QTEST_MAIN(CostUsageServiceTest)

#include "tst_CostUsageService.moc"
