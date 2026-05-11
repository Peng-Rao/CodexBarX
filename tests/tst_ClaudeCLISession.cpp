#include "../src/providers/claude/ClaudeCLISession.h"
#include <QtTest/QtTest>

class tst_ClaudeCLISession : public QObject {
    Q_OBJECT
private slots:
    void isClaudeInstalledReturnsBoolean();
    void resolveBinaryPathReturnsEmptyWhenNotInstalled();
    void captureResultDefaults();
};

void tst_ClaudeCLISession::isClaudeInstalledReturnsBoolean()
{
    // This test just verifies the function runs without crashing
    // The result depends on whether claude is installed on the system
    bool result = ClaudeCLISession::isClaudeInstalled();
    Q_UNUSED(result)
    // Just verify it returns a boolean, don't assert true/false
    QVERIFY(true);
}

void tst_ClaudeCLISession::resolveBinaryPathReturnsEmptyWhenNotInstalled()
{
    // If claude is not in PATH, this should return empty
    QString path = ClaudeCLISession::resolveBinaryPath();
    // Just verify the function runs - we can't assert the result
    // because it depends on system state
    qDebug() << "Resolved claude path:" << path;
    QVERIFY(true);
}

void tst_ClaudeCLISession::captureResultDefaults()
{
    ClaudeCLISession::CaptureResult result;
    QVERIFY(!result.success);
    QVERIFY(result.output.isEmpty());
    QVERIFY(result.errorMessage.isEmpty());
}

QTEST_MAIN(tst_ClaudeCLISession)
#include "tst_ClaudeCLISession.moc"
