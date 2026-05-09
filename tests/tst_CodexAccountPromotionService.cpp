#include <QtTest/QtTest>
#include "providers/codex/CodexAccountPromotionService.h"
#include "providers/codex/CodexAccountPromotionContext.h"

class tst_CodexAccountPromotionService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Result structure tests
    void test_result_defaults();
    void test_result_promoted();
    void test_result_failed();

    // Context tests
    void test_context_invalidAccount();

    // Service state tests
    void test_service_initialState();
    void test_service_promoteWithoutBackend();

private:
};

void tst_CodexAccountPromotionService::initTestCase()
{
}

void tst_CodexAccountPromotionService::cleanupTestCase()
{
}

// ============================================================================
// Result Tests
// ============================================================================

void tst_CodexAccountPromotionService::test_result_defaults()
{
    CodexAccountPromotionResult result;
    QCOMPARE(result.outcome, PromotionOutcome::Failed);
    QCOMPARE(result.displacedDisposition, DisplacedLiveDisposition::None);
    QVERIFY(result.targetManagedAccountId.isEmpty());
    QVERIFY(result.displacedManagedAccountId.isEmpty());
    QVERIFY(!result.didMutateLiveAuth);
    QVERIFY(result.errorMessage.isEmpty());
}

void tst_CodexAccountPromotionService::test_result_promoted()
{
    CodexAccountPromotionResult result;
    result.outcome = PromotionOutcome::Promoted;
    result.targetManagedAccountId = QStringLiteral("acct-123");
    result.displacedDisposition = DisplacedLiveDisposition::Imported;
    result.displacedManagedAccountId = QStringLiteral("acct-456");
    result.didMutateLiveAuth = true;

    QCOMPARE(result.outcome, PromotionOutcome::Promoted);
    QCOMPARE(result.targetManagedAccountId, QStringLiteral("acct-123"));
    QCOMPARE(result.displacedDisposition, DisplacedLiveDisposition::Imported);
    QVERIFY(result.didMutateLiveAuth);
}

void tst_CodexAccountPromotionService::test_result_failed()
{
    CodexAccountPromotionResult result;
    result.outcome = PromotionOutcome::Failed;
    result.errorMessage = QStringLiteral("Test error");

    QCOMPARE(result.outcome, PromotionOutcome::Failed);
    QCOMPARE(result.errorMessage, QStringLiteral("Test error"));
}

// ============================================================================
// Context Tests
// ============================================================================

void tst_CodexAccountPromotionService::test_context_invalidAccount()
{
    ManagedCodexAccountStore store;
    CodexSystemAccountObserver observer;
    QHash<QString, QString> env;

    CodexAccountPromotionContext ctx = CodexAccountPromotionContext::build(
        QStringLiteral("nonexistent-account"), env, store, observer
    );

    QVERIFY(!ctx.valid);
    QVERIFY(!ctx.errorMessage.isEmpty());
}

// ============================================================================
// Service Tests
// ============================================================================

void tst_CodexAccountPromotionService::test_service_initialState()
{
    CodexAccountPromotionService service;
    QVERIFY(!service.isPromoting());
    QVERIFY(service.promotingAccountId().isEmpty());
}

void tst_CodexAccountPromotionService::test_service_promoteWithoutBackend()
{
    CodexAccountPromotionService service;
    QSignalSpy spy(&service, &CodexAccountPromotionService::promotionFinished);

    service.promoteAsync(QStringLiteral("nonexistent-account"));

    QCOMPARE(spy.count(), 1);
    auto result = spy.at(0).at(1).value<CodexAccountPromotionResult>();
    QCOMPARE(result.outcome, PromotionOutcome::Failed);
    QVERIFY(!result.errorMessage.isEmpty());
}

QTEST_MAIN(tst_CodexAccountPromotionService)
#include "tst_CodexAccountPromotionService.moc"
