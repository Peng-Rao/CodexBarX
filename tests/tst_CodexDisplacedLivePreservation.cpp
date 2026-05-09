#include <QtTest/QtTest>
#include "providers/codex/CodexDisplacedLivePreservation.h"

class tst_CodexDisplacedLivePreservation : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Plan tests
    void test_plan_none();
    void test_plan_reject();
    void test_plan_importNew();
    void test_plan_refreshExisting();
    void test_plan_repairExisting();

    // Planner tests
    void test_planner_liveMissing();
    void test_planner_liveMatchesTarget();
    void test_planner_liveHasNoIdentity();

private:
};

void tst_CodexDisplacedLivePreservation::initTestCase()
{
}

void tst_CodexDisplacedLivePreservation::cleanupTestCase()
{
}

// ============================================================================
// Plan Tests
// ============================================================================

void tst_CodexDisplacedLivePreservation::test_plan_none()
{
    auto plan = CodexDisplacedLivePreservationPlan::none(PreservationNoneReason::LiveIsMissing);
    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::None);
    QVERIFY(plan.noneReason().has_value());
    QCOMPARE(plan.noneReason().value(), PreservationNoneReason::LiveIsMissing);
    QVERIFY(!plan.rejectReason().has_value());
    QVERIFY(!plan.importReason().has_value());
}

void tst_CodexDisplacedLivePreservation::test_plan_reject()
{
    auto plan = CodexDisplacedLivePreservationPlan::reject(PreservationRejectReason::LiveIsApiOnly);
    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::Reject);
    QVERIFY(plan.rejectReason().has_value());
    QCOMPARE(plan.rejectReason().value(), PreservationRejectReason::LiveIsApiOnly);
    QVERIFY(!plan.noneReason().has_value());
}

void tst_CodexDisplacedLivePreservation::test_plan_importNew()
{
    PreservationImportReason reason;
    reason.email = QStringLiteral("test@example.com");
    reason.providerAccountId = QStringLiteral("acct-123");

    auto plan = CodexDisplacedLivePreservationPlan::importNew(reason);
    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::ImportNew);
    QVERIFY(plan.importReason().has_value());
    QCOMPARE(plan.importReason()->email, QStringLiteral("test@example.com"));
    QCOMPARE(plan.importReason()->providerAccountId, QStringLiteral("acct-123"));
}

void tst_CodexDisplacedLivePreservation::test_plan_refreshExisting()
{
    PreservationRefreshReason reason;
    reason.existingAccountId = QStringLiteral("managed-456");
    reason.hadStaleCredentials = true;

    auto plan = CodexDisplacedLivePreservationPlan::refreshExisting(reason);
    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::RefreshExisting);
    QVERIFY(plan.refreshReason().has_value());
    QCOMPARE(plan.refreshReason()->existingAccountId, QStringLiteral("managed-456"));
    QVERIFY(plan.refreshReason()->hadStaleCredentials);
}

void tst_CodexDisplacedLivePreservation::test_plan_repairExisting()
{
    PreservationRepairReason reason;
    reason.existingAccountId = QStringLiteral("managed-789");
    reason.missingHomePath = QStringLiteral("/old/missing/path");

    auto plan = CodexDisplacedLivePreservationPlan::repairExisting(reason);
    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::RepairExisting);
    QVERIFY(plan.repairReason().has_value());
    QCOMPARE(plan.repairReason()->existingAccountId, QStringLiteral("managed-789"));
    QCOMPARE(plan.repairReason()->missingHomePath, QStringLiteral("/old/missing/path"));
}

// ============================================================================
// Planner Tests
// ============================================================================

void tst_CodexDisplacedLivePreservation::test_planner_liveMissing()
{
    CodexDisplacedLivePreservationPlanner planner;

    CodexIdentity targetIdentity = CodexIdentity::providerAccount("acct-target");
    QHash<QString, CodexIdentity> existingIdentities;
    QHash<QString, QString> existingHomePaths;

    auto plan = planner.makePlan(
        std::nullopt,  // No live account
        targetIdentity,
        QStringLiteral("/home/.codex-managed"),
        existingIdentities,
        existingHomePaths
    );

    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::None);
    QVERIFY(plan.noneReason().has_value());
    QCOMPARE(plan.noneReason().value(), PreservationNoneReason::LiveIsMissing);
}

void tst_CodexDisplacedLivePreservation::test_planner_liveMatchesTarget()
{
    CodexDisplacedLivePreservationPlanner planner;

    ObservedSystemCodexAccount liveAccount;
    liveAccount.identity = CodexIdentity::providerAccount("acct-same");
    liveAccount.email = QStringLiteral("test@example.com");

    CodexIdentity targetIdentity = CodexIdentity::providerAccount("acct-same");
    QHash<QString, CodexIdentity> existingIdentities;
    QHash<QString, QString> existingHomePaths;

    auto plan = planner.makePlan(
        liveAccount,
        targetIdentity,
        QStringLiteral("/home/.codex-managed"),
        existingIdentities,
        existingHomePaths
    );

    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::None);
    QVERIFY(plan.noneReason().has_value());
    QCOMPARE(plan.noneReason().value(), PreservationNoneReason::LiveMatchesTarget);
}

void tst_CodexDisplacedLivePreservation::test_planner_liveHasNoIdentity()
{
    CodexDisplacedLivePreservationPlanner planner;

    ObservedSystemCodexAccount liveAccount;
    liveAccount.identity = CodexIdentity::unresolved();  // Unresolved identity
    liveAccount.email = QString();  // No email either

    CodexIdentity targetIdentity = CodexIdentity::providerAccount("acct-target");
    QHash<QString, CodexIdentity> existingIdentities;
    QHash<QString, QString> existingHomePaths;

    auto plan = planner.makePlan(
        liveAccount,
        targetIdentity,
        QStringLiteral("/home/.codex-managed"),
        existingIdentities,
        existingHomePaths
    );

    QCOMPARE(plan.kind(), CodexDisplacedLivePreservationPlan::Kind::Reject);
    QVERIFY(plan.rejectReason().has_value());
    QCOMPARE(plan.rejectReason().value(), PreservationRejectReason::LiveHasNoIdentity);
}

QTEST_MAIN(tst_CodexDisplacedLivePreservation)
#include "tst_CodexDisplacedLivePreservation.moc"
