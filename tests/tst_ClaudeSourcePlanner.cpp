#include "../src/providers/claude/ClaudeSourcePlanner.h"

#include <QtTest/QtTest>
#include <QDebug>

class tst_ClaudeSourcePlanner : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void dataSourceToString();
    void dataSourceFromString();
    void planStepDefaults();
    void planAvailableSteps();
    void preferredStepSkipsUnavailableSteps();
    void autoModeAppRuntime();
    void autoModeAppRuntimeFallsBackToCliWhenOAuthMissing();
    void autoModeCliRuntime();
    void autoModeCliRuntimeFallsBackToCliWhenWebMissing();
    void explicitSourceSelectionOnlyPlansSelectedSource_data();
    void explicitSourceSelectionOnlyPlansSelectedSource();
    void noSourceAvailable();
};

void tst_ClaudeSourcePlanner::initTestCase()
{
    qDebug() << "Test initialization";
}

void tst_ClaudeSourcePlanner::dataSourceToString()
{
    qDebug() << "Testing dataSourceToString";
    QCOMPARE(ClaudeSourcePlanner::dataSourceToString(ClaudeDataSource::Auto), QString("auto"));
    QCOMPARE(ClaudeSourcePlanner::dataSourceToString(ClaudeDataSource::OAuth), QString("oauth"));
    QCOMPARE(ClaudeSourcePlanner::dataSourceToString(ClaudeDataSource::CLI), QString("cli"));
    QCOMPARE(ClaudeSourcePlanner::dataSourceToString(ClaudeDataSource::Web), QString("web"));
}

void tst_ClaudeSourcePlanner::dataSourceFromString()
{
    qDebug() << "Testing dataSourceFromString";
    QCOMPARE(ClaudeSourcePlanner::dataSourceFromString("oauth"), ClaudeDataSource::OAuth);
    QCOMPARE(ClaudeSourcePlanner::dataSourceFromString("cli"), ClaudeDataSource::CLI);
    QCOMPARE(ClaudeSourcePlanner::dataSourceFromString("web"), ClaudeDataSource::Web);
    QCOMPARE(ClaudeSourcePlanner::dataSourceFromString("auto"), ClaudeDataSource::Auto);
}

void tst_ClaudeSourcePlanner::planStepDefaults()
{
    qDebug() << "Testing planStepDefaults";
    ClaudeFetchPlanStep step;
    step.dataSource = ClaudeDataSource::OAuth;
    step.isPlausiblyAvailable = false;
    QCOMPARE(step.dataSourceLabel(), QString("oauth"));
}

void tst_ClaudeSourcePlanner::planAvailableSteps()
{
    qDebug() << "Testing planAvailableSteps";
    ClaudeFetchPlan plan;

    ClaudeFetchPlanStep step1;
    step1.dataSource = ClaudeDataSource::OAuth;
    step1.isPlausiblyAvailable = true;

    ClaudeFetchPlanStep step2;
    step2.dataSource = ClaudeDataSource::CLI;
    step2.isPlausiblyAvailable = false;

    plan.orderedSteps = {step1, step2};

    auto available = plan.availableSteps();
    QCOMPARE(available.size(), 1);
    QCOMPARE(available[0].dataSource, ClaudeDataSource::OAuth);
}

void tst_ClaudeSourcePlanner::preferredStepSkipsUnavailableSteps()
{
    qDebug() << "Testing preferredStepSkipsUnavailableSteps";
    ClaudeFetchPlan plan;

    ClaudeFetchPlanStep step1;
    step1.dataSource = ClaudeDataSource::OAuth;
    step1.isPlausiblyAvailable = false;

    ClaudeFetchPlanStep step2;
    step2.dataSource = ClaudeDataSource::CLI;
    step2.isPlausiblyAvailable = true;

    ClaudeFetchPlanStep step3;
    step3.dataSource = ClaudeDataSource::Web;
    step3.isPlausiblyAvailable = true;

    plan.orderedSteps = {step1, step2, step3};

    ClaudeFetchPlanStep* preferred = plan.preferredStep();
    QVERIFY(preferred != nullptr);
    QCOMPARE(preferred->dataSource, ClaudeDataSource::CLI);

    const ClaudeFetchPlan& constPlan = plan;
    const ClaudeFetchPlanStep* constPreferred = constPlan.preferredStep();
    QVERIFY(constPreferred != nullptr);
    QCOMPARE(constPreferred->dataSource, ClaudeDataSource::CLI);
}

void tst_ClaudeSourcePlanner::autoModeAppRuntime()
{
    qDebug() << "Testing autoModeAppRuntime";
    ClaudeSourcePlanningInput input;
    input.selectedSource = ClaudeDataSource::Auto;
    input.hasOAuthCredentials = true;
    input.hasCLI = true;
    input.hasWebSession = true;
    input.isCLIRuntime = false;

    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    QCOMPARE(plan.orderedSteps.size(), 3);
    QCOMPARE(plan.orderedSteps[0].dataSource, ClaudeDataSource::OAuth);
    QCOMPARE(plan.orderedSteps[1].dataSource, ClaudeDataSource::CLI);
    QCOMPARE(plan.orderedSteps[2].dataSource, ClaudeDataSource::Web);
    qDebug() << "autoModeAppRuntime done";
}

void tst_ClaudeSourcePlanner::autoModeAppRuntimeFallsBackToCliWhenOAuthMissing()
{
    qDebug() << "Testing autoModeAppRuntimeFallsBackToCliWhenOAuthMissing";
    ClaudeSourcePlanningInput input;
    input.selectedSource = ClaudeDataSource::Auto;
    input.hasOAuthCredentials = false;
    input.hasCLI = true;
    input.hasWebSession = true;
    input.isCLIRuntime = false;

    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    QCOMPARE(plan.orderedSteps.size(), 3);
    QCOMPARE(plan.orderedSteps[0].dataSource, ClaudeDataSource::OAuth);
    QVERIFY(!plan.orderedSteps[0].isPlausiblyAvailable);
    QCOMPARE(plan.orderedSteps[1].dataSource, ClaudeDataSource::CLI);
    QVERIFY(plan.orderedSteps[1].isPlausiblyAvailable);
    QCOMPARE(plan.orderedSteps[2].dataSource, ClaudeDataSource::Web);
    QVERIFY(plan.orderedSteps[2].isPlausiblyAvailable);

    const ClaudeFetchPlanStep* preferred = plan.preferredStep();
    QVERIFY(preferred != nullptr);
    QCOMPARE(preferred->dataSource, ClaudeDataSource::CLI);
}

void tst_ClaudeSourcePlanner::autoModeCliRuntime()
{
    qDebug() << "Testing autoModeCliRuntime";
    ClaudeSourcePlanningInput input;
    input.selectedSource = ClaudeDataSource::Auto;
    input.hasOAuthCredentials = true;
    input.hasCLI = true;
    input.hasWebSession = true;
    input.isCLIRuntime = true;

    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    QCOMPARE(plan.orderedSteps.size(), 2);
    QCOMPARE(plan.orderedSteps[0].dataSource, ClaudeDataSource::Web);
    QCOMPARE(plan.orderedSteps[0].reason, ClaudeSourcePlanReason::CLIAutoPreferredWeb);
    QVERIFY(plan.orderedSteps[0].isPlausiblyAvailable);
    QCOMPARE(plan.orderedSteps[1].dataSource, ClaudeDataSource::CLI);
    QCOMPARE(plan.orderedSteps[1].reason, ClaudeSourcePlanReason::CLIAutoFallbackCLI);
    QVERIFY(plan.orderedSteps[1].isPlausiblyAvailable);
}

void tst_ClaudeSourcePlanner::autoModeCliRuntimeFallsBackToCliWhenWebMissing()
{
    qDebug() << "Testing autoModeCliRuntimeFallsBackToCliWhenWebMissing";
    ClaudeSourcePlanningInput input;
    input.selectedSource = ClaudeDataSource::Auto;
    input.hasOAuthCredentials = true;
    input.hasCLI = true;
    input.hasWebSession = false;
    input.isCLIRuntime = true;

    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    QCOMPARE(plan.orderedSteps.size(), 2);
    QCOMPARE(plan.orderedSteps[0].dataSource, ClaudeDataSource::Web);
    QVERIFY(!plan.orderedSteps[0].isPlausiblyAvailable);
    QCOMPARE(plan.orderedSteps[1].dataSource, ClaudeDataSource::CLI);
    QVERIFY(plan.orderedSteps[1].isPlausiblyAvailable);

    const ClaudeFetchPlanStep* preferred = plan.preferredStep();
    QVERIFY(preferred != nullptr);
    QCOMPARE(preferred->dataSource, ClaudeDataSource::CLI);
}

void tst_ClaudeSourcePlanner::explicitSourceSelectionOnlyPlansSelectedSource_data()
{
    QTest::addColumn<ClaudeDataSource>("selectedSource");
    QTest::addColumn<bool>("hasOAuthCredentials");
    QTest::addColumn<bool>("hasCLI");
    QTest::addColumn<bool>("hasWebSession");
    QTest::addColumn<bool>("expectedAvailable");

    QTest::newRow("explicit oauth available")
        << ClaudeDataSource::OAuth << true << false << false << true;
    QTest::newRow("explicit oauth unavailable")
        << ClaudeDataSource::OAuth << false << true << true << false;
    QTest::newRow("explicit cli available")
        << ClaudeDataSource::CLI << false << true << false << true;
    QTest::newRow("explicit cli unavailable")
        << ClaudeDataSource::CLI << true << false << true << false;
    QTest::newRow("explicit web available")
        << ClaudeDataSource::Web << false << false << true << true;
    QTest::newRow("explicit web unavailable")
        << ClaudeDataSource::Web << true << true << false << false;
}

void tst_ClaudeSourcePlanner::explicitSourceSelectionOnlyPlansSelectedSource()
{
    QFETCH(ClaudeDataSource, selectedSource);
    QFETCH(bool, hasOAuthCredentials);
    QFETCH(bool, hasCLI);
    QFETCH(bool, hasWebSession);
    QFETCH(bool, expectedAvailable);

    ClaudeSourcePlanningInput input;
    input.selectedSource = selectedSource;
    input.hasOAuthCredentials = hasOAuthCredentials;
    input.hasCLI = hasCLI;
    input.hasWebSession = hasWebSession;
    input.isCLIRuntime = false;

    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    QCOMPARE(plan.orderedSteps.size(), 1);
    QCOMPARE(plan.orderedSteps[0].dataSource, selectedSource);
    QCOMPARE(plan.orderedSteps[0].reason, ClaudeSourcePlanReason::ExplicitSourceSelection);
    QCOMPARE(plan.orderedSteps[0].isPlausiblyAvailable, expectedAvailable);
    QCOMPARE(plan.isNoSourceAvailable(), !expectedAvailable);
}

void tst_ClaudeSourcePlanner::noSourceAvailable()
{
    qDebug() << "Testing noSourceAvailable";
    ClaudeSourcePlanningInput input;
    input.selectedSource = ClaudeDataSource::Auto;
    input.hasOAuthCredentials = false;
    input.hasCLI = false;
    input.hasWebSession = false;
    input.isCLIRuntime = false;

    ClaudeFetchPlan plan = ClaudeSourcePlanner::resolve(input);

    QCOMPARE(plan.orderedSteps.size(), 3);
    QVERIFY(!plan.orderedSteps[0].isPlausiblyAvailable);
    QVERIFY(!plan.orderedSteps[1].isPlausiblyAvailable);
    QVERIFY(!plan.orderedSteps[2].isPlausiblyAvailable);
    QVERIFY(plan.isNoSourceAvailable());
    QVERIFY(plan.preferredStep() == nullptr);
    qDebug() << "noSourceAvailable done";
}

QTEST_MAIN(tst_ClaudeSourcePlanner)
#include "tst_ClaudeSourcePlanner.moc"
