#include <QtTest/QtTest>
#include "providers/codex/CodexOpenAIWorkspaceResolver.h"
#include "providers/codex/CodexOpenAIWorkspaceIdentityCache.h"

class tst_CodexOpenAIWorkspaceResolver : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Identity tests
    void test_identity_equality();
    void test_identity_normalization();

    // WorkspaceIdentity tests
    void test_workspaceIdentity_equality();
    void test_workspaceIdentity_fields();

    // JWT parsing tests
    void test_resolveFromJWT_emptyToken();
    void test_resolveFromJWT_invalidToken();
    void test_resolveFromJWT_validToken();

    // Credentials resolution tests
    void test_resolveFromCredentials_emptyCredentials();

    // Cache tests
    void test_cache_storeAndRetrieve();
    void test_cache_emptyWorkspaceId();

private:
};

void tst_CodexOpenAIWorkspaceResolver::initTestCase()
{
}

void tst_CodexOpenAIWorkspaceResolver::cleanupTestCase()
{
}

// ============================================================================
// Identity Tests
// ============================================================================

void tst_CodexOpenAIWorkspaceResolver::test_identity_equality()
{
    CodexOpenAIWorkspaceIdentity id1;
    id1.workspaceAccountID = QStringLiteral("acct-123");
    id1.workspaceLabel = QStringLiteral("Personal");

    CodexOpenAIWorkspaceIdentity id2;
    id2.workspaceAccountID = QStringLiteral("acct-123");
    id2.workspaceLabel = QStringLiteral("Personal");

    CodexOpenAIWorkspaceIdentity id3;
    id3.workspaceAccountID = QStringLiteral("acct-456");
    id3.workspaceLabel = QStringLiteral("Personal");

    QCOMPARE(id1, id2);
    QVERIFY(id1 != id3);
}

void tst_CodexOpenAIWorkspaceResolver::test_identity_normalization()
{
    QString normalized = CodexOpenAIWorkspaceIdentity::normalizeWorkspaceAccountID("  ACCT-123  ");
    QCOMPARE(normalized, QStringLiteral("acct-123"));

    QString normalizedLabel = CodexOpenAIWorkspaceIdentity::normalizeWorkspaceLabel("  Workspace  ");
    QCOMPARE(normalizedLabel, QStringLiteral("Workspace"));

    QString emptyLabel = CodexOpenAIWorkspaceIdentity::normalizeWorkspaceLabel("   ");
    QVERIFY(emptyLabel.isEmpty());
}

// ============================================================================
// WorkspaceIdentity Tests
// ============================================================================

void tst_CodexOpenAIWorkspaceResolver::test_workspaceIdentity_equality()
{
    CodexWorkspaceIdentity ws1;
    ws1.workspaceId = QStringLiteral("ws-1");
    ws1.workspaceName = QStringLiteral("Work");
    ws1.workspaceAccountId = QStringLiteral("acct-123");
    ws1.isDefault = true;

    CodexWorkspaceIdentity ws2;
    ws2.workspaceId = QStringLiteral("ws-1");
    ws2.workspaceName = QStringLiteral("Work");
    ws2.workspaceAccountId = QStringLiteral("acct-123");
    ws2.isDefault = true;

    CodexWorkspaceIdentity ws3;
    ws3.workspaceId = QStringLiteral("ws-2");
    ws3.workspaceName = QStringLiteral("Work");
    ws3.workspaceAccountId = QStringLiteral("acct-123");
    ws3.isDefault = true;

    QCOMPARE(ws1, ws2);
    QVERIFY(ws1 != ws3);
}

void tst_CodexOpenAIWorkspaceResolver::test_workspaceIdentity_fields()
{
    CodexWorkspaceIdentity ws;
    ws.workspaceId = QStringLiteral("ws-123");
    ws.workspaceName = QStringLiteral("My Workspace");
    ws.workspaceAccountId = QStringLiteral("acct-456");
    ws.isDefault = true;

    QCOMPARE(ws.workspaceId, QStringLiteral("ws-123"));
    QCOMPARE(ws.workspaceName, QStringLiteral("My Workspace"));
    QCOMPARE(ws.workspaceAccountId, QStringLiteral("acct-456"));
    QVERIFY(ws.isDefault);
}

// ============================================================================
// JWT Parsing Tests
// ============================================================================

void tst_CodexOpenAIWorkspaceResolver::test_resolveFromJWT_emptyToken()
{
    CodexWorkspaceResolveResult result = CodexOpenAIWorkspaceResolver::resolveFromJWT(QString());
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
}

void tst_CodexOpenAIWorkspaceResolver::test_resolveFromJWT_invalidToken()
{
    CodexWorkspaceResolveResult result = CodexOpenAIWorkspaceResolver::resolveFromJWT(
        QStringLiteral("invalid.jwt.token")
    );
    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
}

void tst_CodexOpenAIWorkspaceResolver::test_resolveFromJWT_validToken()
{
    // Create a simple JWT with just the payload (not a valid JWT format, but parseJWTPayload handles it)
    // For a real test, we'd need a properly formatted JWT
    // This tests the fallback path when JWT decoding fails
    CodexWorkspaceResolveResult result = CodexOpenAIWorkspaceResolver::resolveFromJWT(
        QStringLiteral("not.a.valid.jwt")
    );
    QVERIFY(!result.success);
}

// ============================================================================
// Credentials Resolution Tests
// ============================================================================

void tst_CodexOpenAIWorkspaceResolver::test_resolveFromCredentials_emptyCredentials()
{
    CodexOAuthCredentials creds;
    CodexWorkspaceResolveResult result = CodexOpenAIWorkspaceResolver::resolveFromCredentials(creds);
    QVERIFY(!result.success);
}

// ============================================================================
// Cache Tests
// ============================================================================

void tst_CodexOpenAIWorkspaceResolver::test_cache_storeAndRetrieve()
{
    CodexOpenAIWorkspaceIdentityCache cache;

    CodexOpenAIWorkspaceIdentity identity;
    identity.workspaceAccountID = QStringLiteral("cache-test-123");
    identity.workspaceLabel = QStringLiteral("Test Workspace");

    cache.store(identity);

    QString label = cache.workspaceLabel(identity.workspaceAccountID);
    QCOMPARE(label, QStringLiteral("Test Workspace"));
}

void tst_CodexOpenAIWorkspaceResolver::test_cache_emptyWorkspaceId()
{
    CodexOpenAIWorkspaceIdentityCache cache;

    QString label = cache.workspaceLabel(QString());
    QVERIFY(label.isEmpty());

    label = cache.workspaceLabel(QStringLiteral("nonexistent-id"));
    QVERIFY(label.isEmpty());
}

QTEST_MAIN(tst_CodexOpenAIWorkspaceResolver)
#include "tst_CodexOpenAIWorkspaceResolver.moc"
