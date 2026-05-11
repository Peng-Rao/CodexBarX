#include "../src/providers/claude/ClaudeStatusProbe.h"
#include "../src/models/ClaudeUsageSnapshot.h"
#include "../src/models/UsageSnapshot.h"
#include <QtTest/QtTest>

class tst_ClaudeStatusProbe : public QObject {
    Q_OBJECT

private slots:
    void parseEmptyString();
    void parseValidUsage();
    void parseMergesAccountInfoFromStatusOutput();
    void parseConvertsUsedPercentToPercentLeft();
    void detectTokenExpired();
    void detectTrustPrompt();
    void detectNotLoggedIn();
    void cliSnapshotPreservesResetDescriptions();
};

void tst_ClaudeStatusProbe::parseEmptyString()
{
    QString empty;
    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(empty);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ClaudeStatusProbe::ParseError::EmptyOutput);
}

void tst_ClaudeStatusProbe::parseValidUsage()
{
    QString output = "Current session\n62% left\nResets in 2h\n"
                     "Current week (all models)\n72% left\nResets Mar 15 at 8am (ET)\n"
                     "Current week (Opus)\n82% left";
    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(output);
    QVERIFY(result.success);
    QVERIFY(result.snapshot.sessionPercentLeft.has_value());
    QCOMPARE(*result.snapshot.sessionPercentLeft, 62);
    QVERIFY(result.snapshot.weeklyPercentLeft.has_value());
    QCOMPARE(*result.snapshot.weeklyPercentLeft, 72);
    QVERIFY(result.snapshot.opusPercentLeft.has_value());
    QCOMPARE(*result.snapshot.opusPercentLeft, 82);
    QCOMPARE(result.snapshot.sessionResetDescription, QStringLiteral("in 2h"));
}

void tst_ClaudeStatusProbe::parseMergesAccountInfoFromStatusOutput()
{
    const QString usage = QStringLiteral(
        "Settings: Usage\n"
        "Current session\n"
        "62% left\n"
        "Current week (all models)\n"
        "72% left\n");
    const QString status = QStringLiteral(
        "Settings: Status\n"
        "Account: person@example.com\n"
        "Organization: Example Org\n"
        "Login Method: Claude Max\n");

    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(usage, status);
    QVERIFY(result.success);
    QVERIFY(result.snapshot.accountEmail.has_value());
    QCOMPARE(*result.snapshot.accountEmail, QStringLiteral("person@example.com"));
    QVERIFY(result.snapshot.accountOrganization.has_value());
    QCOMPARE(*result.snapshot.accountOrganization, QStringLiteral("Example Org"));
    QVERIFY(result.snapshot.loginMethod.has_value());
    QCOMPARE(*result.snapshot.loginMethod, QStringLiteral("Claude Max"));
}

void tst_ClaudeStatusProbe::parseConvertsUsedPercentToPercentLeft()
{
    const QString output = QStringLiteral("Current session\n38% used\n");
    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(output);
    QVERIFY(result.success);
    QVERIFY(result.snapshot.sessionPercentLeft.has_value());
    QCOMPARE(*result.snapshot.sessionPercentLeft, 62);
}

void tst_ClaudeStatusProbe::detectTokenExpired()
{
    const QString output = QStringLiteral("token_expired: refresh your Claude credentials");
    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(output);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ClaudeStatusProbe::ParseError::TokenExpired);
    QVERIFY(result.errorMessage.contains(QStringLiteral("claude login"), Qt::CaseInsensitive));
}

void tst_ClaudeStatusProbe::detectTrustPrompt()
{
    const QString output = QStringLiteral("Do you trust the files in this folder?");
    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(output);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ClaudeStatusProbe::ParseError::TrustPrompt);
}

void tst_ClaudeStatusProbe::detectNotLoggedIn()
{
    QString output = "Not logged in. Run claude login.";
    ClaudeStatusProbe::ParseResult result = ClaudeStatusProbe::parse(output);
    QVERIFY(!result.success);
    QCOMPARE(result.error, ClaudeStatusProbe::ParseError::NotLoggedIn);
}

void tst_ClaudeStatusProbe::cliSnapshotPreservesResetDescriptions()
{
    ClaudeStatusSnapshot cli;
    cli.sessionPercentLeft = 62;
    cli.sessionResetDescription = QStringLiteral("in 2h");
    cli.weeklyPercentLeft = 72;
    cli.weeklyResetDescription = QStringLiteral("Mar 15 at 8am (ET)");
    cli.opusPercentLeft = 82;
    cli.opusResetDescription = QStringLiteral("Mar 15 at 8am (ET)");

    UsageSnapshot usage = ClaudeUsageSnapshot::fromCLIOutput(cli).toUsageSnapshot();
    QVERIFY(usage.primary.has_value());
    QVERIFY(usage.primary->resetDescription.has_value());
    QCOMPARE(*usage.primary->resetDescription, QStringLiteral("in 2h"));
    QVERIFY(!usage.primary->resetsAt.has_value());
    QVERIFY(usage.secondary.has_value());
    QVERIFY(usage.secondary->resetDescription.has_value());
    QCOMPARE(*usage.secondary->resetDescription, QStringLiteral("Mar 15 at 8am (ET)"));
    QVERIFY(usage.tertiary.has_value());
    QVERIFY(usage.tertiary->resetDescription.has_value());
    QCOMPARE(*usage.tertiary->resetDescription, QStringLiteral("Mar 15 at 8am (ET)"));
}

QTEST_MAIN(tst_ClaudeStatusProbe)
#include "tst_ClaudeStatusProbe.moc"
