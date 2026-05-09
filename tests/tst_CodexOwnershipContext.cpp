#include <QtTest/QtTest>

#include "providers/codex/CodexOwnershipContext.h"

class tst_CodexOwnershipContext : public QObject {
    Q_OBJECT

private slots:
    void test_build_basic();
    void test_build_withDashboardFallback();
    void test_isValid_empty();
    void test_isValid_withData();
    void test_hasDashboardFallback();
    void test_multiAccountVeto_blocksContinuity();
    void test_extendedContinuity_withLegacyEmailHash();
    void test_extendedContinuity_withoutVeto();
    void test_contextBasedContinuity();
};

void tst_CodexOwnershipContext::test_build_basic()
{
    CodexOwnershipContext ctx = CodexOwnershipContextBuilder::build(
        "test@example.com",
        "acct_123",
        QDateTime::currentDateTime(),
        false
    );

    QVERIFY(!ctx.canonicalKey.isEmpty());
    QVERIFY(!ctx.canonicalEmailHashKey.isEmpty());
    QVERIFY(ctx.isValid());
    QVERIFY(!ctx.hasAdjacentMultiAccountVeto);
}

void tst_CodexOwnershipContext::test_build_withDashboardFallback()
{
    CodexOwnershipContext ctx = CodexOwnershipContextBuilder::buildWithDashboardFallback(
        "",
        "",
        QDateTime(),
        false,
        "dashboard@example.com"
    );

    // Should use dashboard email as fallback
    QVERIFY(!ctx.planUtilizationLegacyEmailHash.isEmpty());
    QVERIFY(ctx.hasDashboardFallback());
}

void tst_CodexOwnershipContext::test_isValid_empty()
{
    CodexOwnershipContext ctx;
    QVERIFY(!ctx.isValid());
}

void tst_CodexOwnershipContext::test_isValid_withData()
{
    CodexOwnershipContext ctx;
    ctx.canonicalKey = "codex:v1:provider-account:test";
    QVERIFY(ctx.isValid());
}

void tst_CodexOwnershipContext::test_hasDashboardFallback()
{
    CodexOwnershipContext ctx;
    QVERIFY(!ctx.hasDashboardFallback());

    ctx.planUtilizationLegacyEmailHash = "some-hash";
    QVERIFY(ctx.hasDashboardFallback());
}

void tst_CodexOwnershipContext::test_multiAccountVeto_blocksContinuity()
{
    QStringList keys;
    keys << "codex:v1:provider-account:test";

    // Without veto, should pass
    QVERIFY(CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
        keys, "codex:v1:provider-account:test", QString(), QString(), false));

    // With veto, should fail even with matching keys
    QVERIFY(!CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
        keys, "codex:v1:provider-account:test", QString(), QString(), true));
}

void tst_CodexOwnershipContext::test_extendedContinuity_withLegacyEmailHash()
{
    QStringList keys;
    QString legacyHash = CodexHistoryOwnership::canonicalKeyFromEmail("test@example.com");
    keys << legacyHash;

    // Should match with legacy email hash parameter
    QVERIFY(CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
        keys,
        "codex:v1:provider-account:test",
        QString(),
        "test@example.com",
        false
    ));

    // Should not match without legacy email hash
    QVERIFY(!CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
        keys,
        "codex:v1:provider-account:test",
        QString(),
        QString(),
        false
    ));
}

void tst_CodexOwnershipContext::test_extendedContinuity_withoutVeto()
{
    QStringList keys;
    keys << "codex:v1:provider-account:test";

    QVERIFY(CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
        keys,
        "codex:v1:provider-account:test",
        QString(),
        QString(),
        false
    ));
}

void tst_CodexOwnershipContext::test_contextBasedContinuity()
{
    CodexOwnershipContext ctx = CodexOwnershipContextBuilder::build(
        "test@example.com",
        "acct_123"
    );

    QStringList keys;
    keys << ctx.canonicalKey;
    keys << ctx.canonicalEmailHashKey;

    QVERIFY(CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(keys, ctx));

    // Add a non-matching key
    keys << "codex:v1:provider-account:other";
    QVERIFY(!CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(keys, ctx));

    // With veto
    ctx.hasAdjacentMultiAccountVeto = true;
    keys.clear();
    keys << ctx.canonicalKey;
    QVERIFY(!CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(keys, ctx));
}

QTEST_MAIN(tst_CodexOwnershipContext)
#include "tst_CodexOwnershipContext.moc"
