#include <QtTest/QtTest>
#include <QSignalSpy>
#include "providers/codex/ManagedCodexAccountService.h"
#include "providers/codex/CodexIdentity.h"
#include "providers/codex/CodexAccountReconciliation.h"

class tst_ManagedCodexAccountService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // CodexIdentity tests
    void test_identity_providerAccount();
    void test_identity_emailOnly();
    void test_identity_unresolved();
    void test_identity_equality();
    void test_identity_normalizeEmail();

    // CodexIdentityMatcher tests
    void test_matcher_sameProviderAccountId();
    void test_matcher_sameEmail();
    void test_matcher_different();
    void test_matcher_unresolved();
    void test_matcher_withEmailFallback();

    // CodexAccountReconciliation tests
    void test_reconciliation_loadSnapshot();
    void test_reconciliation_resolveActiveSource();

private:
    QHash<QString, QString> m_env;
};

void tst_ManagedCodexAccountService::initTestCase()
{
    // Set up minimal environment
    m_env.insert(QStringLiteral("USERPROFILE"), QDir::tempPath());
    m_env.insert(QStringLiteral("APPDATA"), QDir::tempPath());
}

void tst_ManagedCodexAccountService::cleanupTestCase()
{
}

// ============================================================================
// CodexIdentity Tests
// ============================================================================

void tst_ManagedCodexAccountService::test_identity_providerAccount()
{
    CodexIdentity id = CodexIdentity::providerAccount("acct-123");
    QCOMPARE(id.type(), CodexIdentityType::ProviderAccount);
    QCOMPARE(id.accountId(), QString("acct-123"));
    QVERIFY(id.email().isEmpty());
}

void tst_ManagedCodexAccountService::test_identity_emailOnly()
{
    CodexIdentity id = CodexIdentity::emailOnly("Test@Example.com");
    QCOMPARE(id.type(), CodexIdentityType::EmailOnly);
    QCOMPARE(id.email(), QString("test@example.com"));  // Normalized
    QVERIFY(id.accountId().isEmpty());
}

void tst_ManagedCodexAccountService::test_identity_unresolved()
{
    CodexIdentity id = CodexIdentity::unresolved();
    QCOMPARE(id.type(), CodexIdentityType::Unresolved);
    QVERIFY(id.accountId().isEmpty());
    QVERIFY(id.email().isEmpty());
}

void tst_ManagedCodexAccountService::test_identity_equality()
{
    CodexIdentity id1 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id2 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id3 = CodexIdentity::providerAccount("acct-456");
    CodexIdentity id4 = CodexIdentity::emailOnly("test@example.com");
    CodexIdentity id5 = CodexIdentity::emailOnly("test@example.com");
    CodexIdentity id6 = CodexIdentity::unresolved();

    QCOMPARE(id1, id2);
    QVERIFY(id1 != id3);
    QVERIFY(id1 != id4);
    QCOMPARE(id4, id5);
    QCOMPARE(id6, CodexIdentity::unresolved());
}

void tst_ManagedCodexAccountService::test_identity_normalizeEmail()
{
    QCOMPARE(CodexIdentity::normalizeEmail("  Test@Example.COM  "), QString("test@example.com"));
    QCOMPARE(CodexIdentity::normalizeEmail(""), QString());
}

// ============================================================================
// CodexIdentityMatcher Tests
// ============================================================================

void tst_ManagedCodexAccountService::test_matcher_sameProviderAccountId()
{
    CodexIdentity id1 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id2 = CodexIdentity::providerAccount("acct-123");
    QVERIFY(CodexIdentityMatcher::matches(id1, id2));
}

void tst_ManagedCodexAccountService::test_matcher_sameEmail()
{
    CodexIdentity id1 = CodexIdentity::emailOnly("test@example.com");
    CodexIdentity id2 = CodexIdentity::emailOnly("test@example.com");
    QVERIFY(CodexIdentityMatcher::matches(id1, id2));
}

void tst_ManagedCodexAccountService::test_matcher_different()
{
    CodexIdentity id1 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id2 = CodexIdentity::providerAccount("acct-456");
    QVERIFY(!CodexIdentityMatcher::matches(id1, id2));

    CodexIdentity id3 = CodexIdentity::emailOnly("test1@example.com");
    CodexIdentity id4 = CodexIdentity::emailOnly("test2@example.com");
    QVERIFY(!CodexIdentityMatcher::matches(id3, id4));

    // Different types never match
    CodexIdentity id5 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id6 = CodexIdentity::emailOnly("test@example.com");
    QVERIFY(!CodexIdentityMatcher::matches(id5, id6));
}

void tst_ManagedCodexAccountService::test_matcher_unresolved()
{
    CodexIdentity unresolved = CodexIdentity::unresolved();
    CodexIdentity provider = CodexIdentity::providerAccount("acct-123");

    // Unresolved never matches anything (including itself)
    QVERIFY(!CodexIdentityMatcher::matches(unresolved, unresolved));
    QVERIFY(!CodexIdentityMatcher::matches(unresolved, provider));
    QVERIFY(!CodexIdentityMatcher::matches(provider, unresolved));
}

void tst_ManagedCodexAccountService::test_matcher_withEmailFallback()
{
    // Same provider account ID - should match
    CodexIdentity id1 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id2 = CodexIdentity::providerAccount("acct-123");
    QVERIFY(CodexIdentityMatcher::matches(id1, id2, QString(), QString()));

    // Different provider account IDs but same email - should match
    CodexIdentity id3 = CodexIdentity::providerAccount("acct-123");
    CodexIdentity id4 = CodexIdentity::providerAccount("acct-456");
    QVERIFY(CodexIdentityMatcher::matches(id3, id4, "test@example.com", "test@example.com"));

    // Different provider account IDs and different emails - should not match
    QVERIFY(!CodexIdentityMatcher::matches(id3, id4, "test1@example.com", "test2@example.com"));
}

// ============================================================================
// CodexAccountReconciliation Tests
// ============================================================================

void tst_ManagedCodexAccountService::test_reconciliation_loadSnapshot()
{
    CodexAccountReconciliation reconciliation(m_env);
    CodexAccountReconciliationSnapshot snapshot = reconciliation.loadSnapshot();

    // Snapshot should be valid even with no accounts
    QVERIFY(!snapshot.hasUnreadableAddedAccountStore);
}

void tst_ManagedCodexAccountService::test_reconciliation_resolveActiveSource()
{
    CodexAccountReconciliation reconciliation(m_env);
    CodexAccountReconciliationSnapshot snapshot = reconciliation.loadSnapshot();

    CodexResolvedActiveSource resolved = reconciliation.resolveActiveSource(snapshot);
    QCOMPARE(resolved.persistedSource, CodexActiveSource::LiveSystem);
    QCOMPARE(resolved.resolvedSource, CodexActiveSource::LiveSystem);
    QVERIFY(!resolved.requiresPersistenceCorrection());
}

QTEST_MAIN(tst_ManagedCodexAccountService)
#include "tst_ManagedCodexAccountService.moc"
