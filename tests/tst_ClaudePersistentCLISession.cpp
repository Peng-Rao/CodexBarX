#include "../src/providers/claude/ClaudePersistentCLISession.h"
#include "../src/providers/shared/ConPTYSession.h"

#include <QtTest/QtTest>
#include <QFileInfo>
#include <QProcess>

class tst_ClaudePersistentCLISession : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Singleton and lifecycle
    void instanceReturnsSingleton();
    void resetClearsSession();
    void isActiveReturnsFalseInitially();

    // CaptureResult defaults
    void captureResultDefaults();
    void usageStatusResultDefaults();

    // Integration tests (require claude CLI installed)
    void captureUsageRequiresClaudeInstalled();
    void captureStatusRequiresClaudeInstalled();
    void captureUsageAndStatusReusesSession();
    void sessionPersistsAcrossCaptures();

private:
    bool shouldRunCliIntegration() const;

    bool m_claudeInstalled = false;
    QString m_claudePath;
};

void tst_ClaudePersistentCLISession::initTestCase()
{
    // Check if claude is installed
    m_claudePath = qEnvironmentVariable("CODEXBAR_CLAUDE_PATH");
    if (m_claudePath.isEmpty()) {
        // Try to find in PATH
        QProcess pathCheck;
        pathCheck.start("where", {"claude"});
        pathCheck.waitForFinished(3000);
        if (pathCheck.exitCode() == 0) {
            m_claudePath = QString::fromUtf8(pathCheck.readAllStandardOutput()).trimmed().split('\n').first();
        }
    }
    m_claudeInstalled = !m_claudePath.isEmpty() && QFileInfo::exists(m_claudePath);

    qDebug() << "Claude CLI installed:" << m_claudeInstalled;
    qDebug() << "Claude path:" << (m_claudePath.isEmpty() ? "(not found)" : m_claudePath);
    qDebug() << "ConPTY available:" << ConPTYSession::isConPtyAvailable();

    // Reset session before tests
    ClaudePersistentCLISession::instance().reset();
}

void tst_ClaudePersistentCLISession::cleanupTestCase()
{
    // Clean up session after tests
    ClaudePersistentCLISession::instance().reset();
}

bool tst_ClaudePersistentCLISession::shouldRunCliIntegration() const
{
    return qEnvironmentVariableIntValue("CODEXBAR_RUN_CLAUDE_CLI_E2E") == 1;
}

void tst_ClaudePersistentCLISession::instanceReturnsSingleton()
{
    ClaudePersistentCLISession& instance1 = ClaudePersistentCLISession::instance();
    ClaudePersistentCLISession& instance2 = ClaudePersistentCLISession::instance();
    QCOMPARE(&instance1, &instance2);
}

void tst_ClaudePersistentCLISession::resetClearsSession()
{
    ClaudePersistentCLISession::instance().reset();
    QVERIFY(!ClaudePersistentCLISession::instance().isActive());
}

void tst_ClaudePersistentCLISession::isActiveReturnsFalseInitially()
{
    ClaudePersistentCLISession::instance().reset();
    QVERIFY(!ClaudePersistentCLISession::instance().isActive());
}

void tst_ClaudePersistentCLISession::captureResultDefaults()
{
    ClaudePersistentCLISession::CaptureResult result;
    QVERIFY(!result.success);
    QVERIFY(result.output.isEmpty());
    QVERIFY(result.errorMessage.isEmpty());
}

void tst_ClaudePersistentCLISession::usageStatusResultDefaults()
{
    ClaudePersistentCLISession::UsageStatusResult result;
    QVERIFY(!result.success);
    QVERIFY(result.usageOutput.isEmpty());
    QVERIFY(result.statusOutput.isEmpty());
    QVERIFY(result.errorMessage.isEmpty());
}

void tst_ClaudePersistentCLISession::captureUsageRequiresClaudeInstalled()
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

    ClaudePersistentCLISession::instance().reset();

    QHash<QString, QString> env;
    auto result = ClaudePersistentCLISession::instance().captureUsage(
        m_claudePath, 120, 30, 30000, env);

    if (result.success) {
        QVERIFY(!result.output.isEmpty());
        qDebug() << "Captured usage output length:" << result.output.length();
    } else {
        qDebug() << "Capture failed (expected if not logged in):" << result.errorMessage;
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

void tst_ClaudePersistentCLISession::captureStatusRequiresClaudeInstalled()
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

    ClaudePersistentCLISession::instance().reset();

    QHash<QString, QString> env;
    auto result = ClaudePersistentCLISession::instance().captureStatus(
        m_claudePath, 120, 30, 15000, env);

    if (result.success) {
        QVERIFY(!result.output.isEmpty());
        qDebug() << "Captured status output length:" << result.output.length();
    } else {
        qDebug() << "Status capture failed (expected if not logged in):" << result.errorMessage;
        QVERIFY(!result.errorMessage.isEmpty());
    }
}

void tst_ClaudePersistentCLISession::captureUsageAndStatusReusesSession()
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

    ClaudePersistentCLISession::instance().reset();

    QHash<QString, QString> env;

    // Capture both usage and status
    auto result = ClaudePersistentCLISession::instance().captureUsageAndStatus(
        m_claudePath, 120, 30, 45000, env);

    // After capture, session should be active
    if (result.success || ClaudePersistentCLISession::instance().isActive()) {
        qDebug() << "Session active after combined capture:" << ClaudePersistentCLISession::instance().isActive();
    }

    if (result.success) {
        QVERIFY(!result.usageOutput.isEmpty());
        qDebug() << "Captured usage/status output lengths:"
                 << result.usageOutput.length() << result.statusOutput.length();
    } else {
        qDebug() << "Combined capture failed (expected if not logged in):" << result.errorMessage;
    }
}

void tst_ClaudePersistentCLISession::sessionPersistsAcrossCaptures()
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

    ClaudePersistentCLISession::instance().reset();

    QHash<QString, QString> env;

    // First capture
    auto result1 = ClaudePersistentCLISession::instance().captureUsage(
        m_claudePath, 120, 30, 30000, env);

    bool activeAfter1st = ClaudePersistentCLISession::instance().isActive();

    // Second capture - should reuse session
    auto result2 = ClaudePersistentCLISession::instance().captureUsage(
        m_claudePath, 120, 30, 20000, env);

    bool activeAfter2nd = ClaudePersistentCLISession::instance().isActive();

    qDebug() << "Session active after 1st capture:" << activeAfter1st;
    qDebug() << "Session active after 2nd capture:" << activeAfter2nd;

    // If first capture succeeded, session should have been active
    if (result1.success) {
        QVERIFY(activeAfter1st);
    }
}

QTEST_MAIN(tst_ClaudePersistentCLISession)
#include "tst_ClaudePersistentCLISession.moc"
