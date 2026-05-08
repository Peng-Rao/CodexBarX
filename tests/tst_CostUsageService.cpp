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

QTEST_MAIN(CostUsageServiceTest)

#include "tst_CostUsageService.moc"
