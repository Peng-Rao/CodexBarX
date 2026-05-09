#include <QtTest/QtTest>
#include "providers/codex/CodexHistoryOwnership.h"

class tst_CodexHistoryOwnership : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Key generation tests
    void test_canonicalKey();
    void test_canonicalKey_empty();
    void test_canonicalKey_normalization();

    void test_canonicalKeyFromEmail();
    void test_canonicalKeyFromEmail_empty();
    void test_canonicalKeyFromEmail_normalization();

    // Classification tests
    void test_classifyPersistedKey_canonical();
    void test_classifyPersistedKey_emailHash();
    void test_classifyPersistedKey_legacyOpaque();
    void test_classifyPersistedKey_unscoped();

    // Continuity tests
    void test_belongsToTargetContinuity_canonicalMatch();
    void test_belongsToTargetContinuity_emailHashMatch();
    void test_belongsToTargetContinuity_noMatch();
    void test_belongsToTargetContinuity_empty();

    void test_hasStrictSingleAccountContinuity_allMatch();
    void test_hasStrictSingleAccountContinuity_partialMatch();
    void test_hasStrictSingleAccountContinuity_empty();

    // SHA256 test
    void test_sha256();

private:
};

void tst_CodexHistoryOwnership::initTestCase()
{
}

void tst_CodexHistoryOwnership::cleanupTestCase()
{
}

// ============================================================================
// Key Generation Tests
// ============================================================================

void tst_CodexHistoryOwnership::test_canonicalKey()
{
    QString key = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));
    QCOMPARE(key, QStringLiteral("codex:v1:provider-account:acct-123"));
}

void tst_CodexHistoryOwnership::test_canonicalKey_empty()
{
    QString key = CodexHistoryOwnership::canonicalKey(QString());
    QVERIFY(key.isEmpty());

    key = CodexHistoryOwnership::canonicalKey(QStringLiteral("   "));
    QVERIFY(key.isEmpty());
}

void tst_CodexHistoryOwnership::test_canonicalKey_normalization()
{
    QString key1 = CodexHistoryOwnership::canonicalKey(QStringLiteral("ACCT-123"));
    QString key2 = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));
    QCOMPARE(key1, key2);

    QString key3 = CodexHistoryOwnership::canonicalKey(QStringLiteral("  ACCT-123  "));
    QCOMPARE(key3, key2);
}

void tst_CodexHistoryOwnership::test_canonicalKeyFromEmail()
{
    QString key = CodexHistoryOwnership::canonicalKeyFromEmail(QStringLiteral("test@example.com"));
    QVERIFY(key.startsWith(QStringLiteral("codex:v1:email-hash:")));
    QVERIFY(key.length() > 20); // Should have hash suffix
}

void tst_CodexHistoryOwnership::test_canonicalKeyFromEmail_empty()
{
    QString key = CodexHistoryOwnership::canonicalKeyFromEmail(QString());
    QVERIFY(key.isEmpty());

    key = CodexHistoryOwnership::canonicalKeyFromEmail(QStringLiteral("   "));
    QVERIFY(key.isEmpty());
}

void tst_CodexHistoryOwnership::test_canonicalKeyFromEmail_normalization()
{
    QString key1 = CodexHistoryOwnership::canonicalKeyFromEmail(QStringLiteral("Test@Example.COM"));
    QString key2 = CodexHistoryOwnership::canonicalKeyFromEmail(QStringLiteral("test@example.com"));
    QCOMPARE(key1, key2);
}

// ============================================================================
// Classification Tests
// ============================================================================

void tst_CodexHistoryOwnership::test_classifyPersistedKey_canonical()
{
    auto [owner, value] = CodexHistoryOwnership::classifyPersistedKey(
        QStringLiteral("codex:v1:provider-account:acct-123"));

    QCOMPARE(owner, CodexHistoryPersistedOwner::Canonical);
    QCOMPARE(value, QStringLiteral("acct-123"));
}

void tst_CodexHistoryOwnership::test_classifyPersistedKey_emailHash()
{
    QString email = QStringLiteral("test@example.com");
    QString hashKey = CodexHistoryOwnership::canonicalKeyFromEmail(email);

    auto [owner, value] = CodexHistoryOwnership::classifyPersistedKey(hashKey);

    QCOMPARE(owner, CodexHistoryPersistedOwner::LegacyEmailHash);
    QVERIFY(!value.isEmpty());
}

void tst_CodexHistoryOwnership::test_classifyPersistedKey_legacyOpaque()
{
    auto [owner, value] = CodexHistoryOwnership::classifyPersistedKey(
        QStringLiteral("legacy:scoped:key"));

    QCOMPARE(owner, CodexHistoryPersistedOwner::LegacyOpaqueScoped);
    QCOMPARE(value, QStringLiteral("legacy:scoped:key"));
}

void tst_CodexHistoryOwnership::test_classifyPersistedKey_unscoped()
{
    auto [owner, value] = CodexHistoryOwnership::classifyPersistedKey(
        QStringLiteral("simplekey"));

    QCOMPARE(owner, CodexHistoryPersistedOwner::LegacyUnscoped);
    QCOMPARE(value, QStringLiteral("simplekey"));
}

// ============================================================================
// Continuity Tests
// ============================================================================

void tst_CodexHistoryOwnership::test_belongsToTargetContinuity_canonicalMatch()
{
    QString canonicalKey = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));

    QVERIFY(CodexHistoryOwnership::belongsToTargetContinuity(
        canonicalKey, canonicalKey, QString()));
}

void tst_CodexHistoryOwnership::test_belongsToTargetContinuity_emailHashMatch()
{
    QString email = QStringLiteral("test@example.com");
    QString emailHashKey = CodexHistoryOwnership::canonicalKeyFromEmail(email);
    QString canonicalKey = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));

    QVERIFY(CodexHistoryOwnership::belongsToTargetContinuity(
        emailHashKey, canonicalKey, emailHashKey));
}

void tst_CodexHistoryOwnership::test_belongsToTargetContinuity_noMatch()
{
    QString key1 = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));
    QString key2 = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-456"));

    QVERIFY(!CodexHistoryOwnership::belongsToTargetContinuity(key1, key2, QString()));
}

void tst_CodexHistoryOwnership::test_belongsToTargetContinuity_empty()
{
    QString key = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));

    QVERIFY(!CodexHistoryOwnership::belongsToTargetContinuity(QString(), key, QString()));
    QVERIFY(!CodexHistoryOwnership::belongsToTargetContinuity(key, QString(), QString()));
}

void tst_CodexHistoryOwnership::test_hasStrictSingleAccountContinuity_allMatch()
{
    QString canonicalKey = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));
    QString emailHashKey = CodexHistoryOwnership::canonicalKeyFromEmail(QStringLiteral("test@example.com"));

    QStringList keys;
    keys << canonicalKey << canonicalKey;

    QVERIFY(CodexHistoryOwnership::hasStrictSingleAccountContinuity(
        keys, canonicalKey, emailHashKey));
}

void tst_CodexHistoryOwnership::test_hasStrictSingleAccountContinuity_partialMatch()
{
    QString canonicalKey1 = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));
    QString canonicalKey2 = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-456"));

    QStringList keys;
    keys << canonicalKey1 << canonicalKey2;

    QVERIFY(!CodexHistoryOwnership::hasStrictSingleAccountContinuity(
        keys, canonicalKey1, QString()));
}

void tst_CodexHistoryOwnership::test_hasStrictSingleAccountContinuity_empty()
{
    QStringList emptyKeys;
    QString canonicalKey = CodexHistoryOwnership::canonicalKey(QStringLiteral("acct-123"));

    QVERIFY(CodexHistoryOwnership::hasStrictSingleAccountContinuity(
        emptyKeys, canonicalKey, QString()));
}

// ============================================================================
// SHA256 Tests
// ============================================================================

void tst_CodexHistoryOwnership::test_sha256()
{
    QString email = QStringLiteral("test@example.com");

    QString key1 = CodexHistoryOwnership::canonicalKeyFromEmail(email);
    QString key2 = CodexHistoryOwnership::canonicalKeyFromEmail(email);

    // Same input should produce same hash
    QCOMPARE(key1, key2);

    // Hash should be consistent length
    QVERIFY(key1.startsWith(QStringLiteral("codex:v1:email-hash:")));
    QString hashPart = key1.mid(QStringLiteral("codex:v1:email-hash:").length());
    QCOMPARE(hashPart.length(), 64); // SHA256 produces 64 hex chars
}

QTEST_MAIN(tst_CodexHistoryOwnership)
#include "tst_CodexHistoryOwnership.moc"
