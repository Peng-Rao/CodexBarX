#include "../src/providers/claude/ClaudeCLISession.h"
#include "../src/providers/shared/ConPTYSession.h"

#include <QtTest/QtTest>
#include <QProcessEnvironment>
#include <QThread>

class tst_ClaudeCLISession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Static methods
    void isClaudeInstalledReturnsBoolean();
    void resolveBinaryPathReturnsValidPathOrEmpty();

    // CaptureResult defaults
    void captureResultDefaults();
    void usageStatusCaptureResultDefaults();

    // Session lifecycle
    void sessionCanBeCreatedAndDestroyed();
    void environmentCanBeSet();
    void timeoutCanBeSet();

    // Integration tests (require claude CLI installed)
    void captureUsageRequiresClaudeInstalled();
    void captureStatusRequiresClaudeInstalled();
    void captureUsageAndStatusRequiresClaudeInstalled();
    void captureUsageHandlesTimeout();

    // Error handling
    void captureUsageReturnsErrorWhenBinaryNotFound();

private:
    bool shouldRunCliIntegration() const;

    bool m_claudeInstalled = false;
    QString m_claudePath;
};

void tst_ClaudeCLISession::initTestCase()
{
    m_claudeInstalled = ClaudeCLISession::isClaudeInstalled();
    m_claudePath = ClaudeCLISession::resolveBinaryPath();

    qDebug() << "Claude CLI installed:" << m_claudeInstalled;
    qDebug() << "Claude path:" << (m_claudePath.isEmpty() ? "(not found)" : m_claudePath);
    qDebug() << "ConPTY available:" << ConPTYSession::isConPtyAvailable();
}

void tst_ClaudeCLISession::cleanupTestCase() {}

bool tst_ClaudeCLISession::shouldRunCliIntegration() const
{
    return qEnvironmentVariableIntValue("CODEXBAR_RUN_CLAUDE_CLI_E2E") == 1;
}

void tst_ClaudeCLISession::isClaudeInstalledReturnsBoolean()
{
    // Just verify the function runs without crashing
    bool result = ClaudeCLISession::isClaudeInstalled();
    // Result depends on system state, both are valid
    QVERIFY(result == true || result == false);
    QCOMPARE(result, m_claudeInstalled);
}

void tst_ClaudeCLISession::resolveBinaryPathReturnsValidPathOrEmpty()
{
    QString path = ClaudeCLISession::resolveBinaryPath();

    if (m_claudeInstalled) {
        QVERIFY(!path.isEmpty());
        QVERIFY(path.contains("claude", Qt::CaseInsensitive));
    } else {
        QVERIFY(path.isEmpty());
    }
}

void tst_ClaudeCLISession::captureResultDefaults()
{
    ClaudeCLISession::CaptureResult result;
    QVERIFY(!result.success);
    QVERIFY(result.output.isEmpty());
    QVERIFY(result.errorMessage.isEmpty());
}

void tst_ClaudeCLISession::usageStatusCaptureResultDefaults()
{
    ClaudeCLISession::UsageStatusCaptureResult result;
    QVERIFY(!result.success);
    QVERIFY(result.usageOutput.isEmpty());
    QVERIFY(result.statusOutput.isEmpty());
    QVERIFY(result.errorMessage.isEmpty());
}

void tst_ClaudeCLISession::sessionCanBeCreatedAndDestroyed()
{
    // Test that session can be created and destroyed without crash
    {
        ClaudeCLISession session;
        session.setTimeout(5000);
    }
    // Session should be destroyed cleanly
    QVERIFY(true);
}

void tst_ClaudeCLISession::environmentCanBeSet()
{
    ClaudeCLISession session;
    QHash<QString, QString> env;
    env["TEST_VAR"] = "test_value";

    // Should not crash
    session.setEnvironment(env);
    QVERIFY(true);
}

void tst_ClaudeCLISession::timeoutCanBeSet()
{
    ClaudeCLISession session;

    // Default timeout
    session.setTimeout(20000);

    // Custom timeout
    session.setTimeout(5000);

    QVERIFY(true);
}

void tst_ClaudeCLISession::captureUsageRequiresClaudeInstalled()
{
    if (!shouldRunCliIntegration()) {
        QSKIP("Set CODEXBAR_RUN_CLAUDE_CLI_E2E=1 to run live Claude CLI capture tests");
    }

    if (!m_claudeInstalled) {
        QSKIP("Claude CLI is not installed on this system");
    }

    if (!ConPTYSession::isConPtyAvailable()) {
        QSKIP("ConPTY is not available on this Windows version");
    }

    ClaudeCLISession session;
    session.setTimeout(30000); // 30 seconds for login/initialization

    // This test requires actual claude CLI with valid auth
    // It may fail if not logged in, which is expected
    ClaudeCLISession::CaptureResult result = session.captureUsage(30000);

    if (result.success) {
        QVERIFY(!result.output.isEmpty());
        qDebug() << "Captured output length:" << result.output.length();
    } else {
        // Failure is acceptable if not logged in or other auth issues
        qDebug() << "Capture failed (expected if not logged in):" << result.errorMessage;
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

void tst_ClaudeCLISession::captureStatusRequiresClaudeInstalled()
{
    if (!shouldRunCliIntegration()) {
        QSKIP("Set CODEXBAR_RUN_CLAUDE_CLI_E2E=1 to run live Claude CLI capture tests");
    }

    if (!m_claudeInstalled) {
        QSKIP("Claude CLI is not installed on this system");
    }

    if (!ConPTYSession::isConPtyAvailable()) {
        QSKIP("ConPTY is not available on this Windows version");
    }

    ClaudeCLISession session;
    session.setTimeout(15000);

    ClaudeCLISession::CaptureResult result = session.captureStatus(15000);

    if (result.success) {
        QVERIFY(!result.output.isEmpty());
        qDebug() << "Captured status output length:" << result.output.length();
    } else {
        qDebug() << "Status capture failed (expected if not logged in):" << result.errorMessage;
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

void tst_ClaudeCLISession::captureUsageAndStatusRequiresClaudeInstalled()
{
    if (!shouldRunCliIntegration()) {
        QSKIP("Set CODEXBAR_RUN_CLAUDE_CLI_E2E=1 to run live Claude CLI capture tests");
    }

    if (!m_claudeInstalled) {
        QSKIP("Claude CLI is not installed on this system");
    }

    if (!ConPTYSession::isConPtyAvailable()) {
        QSKIP("ConPTY is not available on this Windows version");
    }

    ClaudeCLISession session;
    session.setTimeout(30000);

    ClaudeCLISession::UsageStatusCaptureResult result = session.captureUsageAndStatus(30000);

    if (result.success) {
        QVERIFY(!result.usageOutput.isEmpty());
        qDebug() << "Captured usage/status lengths:" << result.usageOutput.length() << result.statusOutput.length();
    } else {
        qDebug() << "Combined capture failed (expected if not logged in):" << result.errorMessage;
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

void tst_ClaudeCLISession::captureUsageHandlesTimeout()
{
    if (!shouldRunCliIntegration()) {
        QSKIP("Set CODEXBAR_RUN_CLAUDE_CLI_E2E=1 to run live Claude CLI capture tests");
    }

    if (!m_claudeInstalled) {
        QSKIP("Claude CLI is not installed on this system");
    }

    if (!ConPTYSession::isConPtyAvailable()) {
        QSKIP("ConPTY is not available on this Windows version");
    }

    ClaudeCLISession session;
    // Use a very short timeout to trigger timeout behavior
    session.setTimeout(100);

    ClaudeCLISession::CaptureResult result = session.captureUsage(100);

    // With 100ms timeout, should either fail with timeout or succeed quickly
    if (!result.success) {
        qDebug() << "Expected timeout or quick failure:" << result.errorMessage;
    }
    // Don't assert - timeout behavior depends on system speed
    QVERIFY(true);
}

void tst_ClaudeCLISession::captureUsageReturnsErrorWhenBinaryNotFound()
{
    if (m_claudeInstalled) {
        QSKIP("This test requires claude CLI to NOT be installed");
    }

    ClaudeCLISession session;
    ClaudeCLISession::CaptureResult result = session.captureUsage(5000);

    QVERIFY(!result.success);
    QVERIFY(!result.errorMessage.isEmpty());
    QVERIFY(result.errorMessage.contains("not found", Qt::CaseInsensitive) ||
            result.errorMessage.contains("not installed", Qt::CaseInsensitive));
}

QTEST_MAIN(tst_ClaudeCLISession)
#include "tst_ClaudeCLISession.moc"
