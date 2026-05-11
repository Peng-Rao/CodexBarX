#include "../src/providers/claude/ClaudeCredentialRouting.h"
#include "../src/account/TokenAccountCredentials.h"

#include <QtTest/QtTest>

class tst_ClaudeCredentialRouting : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void resolveWithOAuth();
    void resolveWithWebCookie();
    void resolveWithEmpty();
    void resolveWithInvalid();
    void fromTokenAccountCredentialsOAuth();
    void fromTokenAccountCredentialsWeb();
    void fromTokenAccountCredentialsEmpty();
    void cookieHeader();
    void sourceLabel();
};

void tst_ClaudeCredentialRouting::initTestCase()
{
}

void tst_ClaudeCredentialRouting::resolveWithOAuth()
{
    auto routing = ClaudeCredentialRouting::resolve(
        QStringLiteral("sk-ant-oat-1234567890abcdef"),
        QString());

    QVERIFY(routing.isValid());
    QCOMPARE(routing.kind(), ClaudeCredentialRouting::Kind::OAuth);
    QCOMPARE(routing.oauthAccessToken(), QStringLiteral("sk-ant-oat-1234567890abcdef"));
    QVERIFY(routing.webCookieValue().isEmpty());
}

void tst_ClaudeCredentialRouting::resolveWithWebCookie()
{
    auto routing = ClaudeCredentialRouting::resolve(
        QString(),
        QStringLiteral("sk-ant-session123456"));

    QVERIFY(routing.isValid());
    QCOMPARE(routing.kind(), ClaudeCredentialRouting::Kind::WebCookie);
    QCOMPARE(routing.webCookieValue(), QStringLiteral("sk-ant-session123456"));
    QVERIFY(routing.oauthAccessToken().isEmpty());
}

void tst_ClaudeCredentialRouting::resolveWithEmpty()
{
    auto routing = ClaudeCredentialRouting::resolve(QString(), QString());

    QVERIFY(!routing.isValid());
    QCOMPARE(routing.kind(), ClaudeCredentialRouting::Kind::None);
}

void tst_ClaudeCredentialRouting::resolveWithInvalid()
{
    // Non-OAuth token without sk-ant-oat prefix
    auto routing = ClaudeCredentialRouting::resolve(
        QStringLiteral("invalid-token"),
        QString());

    QVERIFY(!routing.isValid());
}

void tst_ClaudeCredentialRouting::fromTokenAccountCredentialsOAuth()
{
    TokenAccountCredentials creds;
    OAuthCredentials oauth;
    oauth.accessToken = SecureString(QStringLiteral("sk-ant-oat-test123"));
    creds.oauth = oauth;

    auto routing = ClaudeCredentialRouting::fromTokenAccountCredentials(creds);

    QVERIFY(routing.isValid());
    QCOMPARE(routing.kind(), ClaudeCredentialRouting::Kind::OAuth);
    QCOMPARE(routing.oauthAccessToken(), QStringLiteral("sk-ant-oat-test123"));
}

void tst_ClaudeCredentialRouting::fromTokenAccountCredentialsWeb()
{
    TokenAccountCredentials creds;
    WebCredentials web;
    web.cookieName = QStringLiteral("sessionKey");
    web.cookieValue = SecureString(QStringLiteral("sk-ant-web123"));
    web.cookieDomain = QStringLiteral("claude.ai");
    creds.web = web;

    auto routing = ClaudeCredentialRouting::fromTokenAccountCredentials(creds);

    QVERIFY(routing.isValid());
    QCOMPARE(routing.kind(), ClaudeCredentialRouting::Kind::WebCookie);
    QCOMPARE(routing.webCookieValue(), QStringLiteral("sk-ant-web123"));
}

void tst_ClaudeCredentialRouting::fromTokenAccountCredentialsEmpty()
{
    TokenAccountCredentials creds;

    auto routing = ClaudeCredentialRouting::fromTokenAccountCredentials(creds);

    QVERIFY(!routing.isValid());
}

void tst_ClaudeCredentialRouting::cookieHeader()
{
    auto routing = ClaudeCredentialRouting::resolve(
        QString(),
        QStringLiteral("sk-ant-session123"));

    QCOMPARE(routing.cookieHeader(), QStringLiteral("sessionKey=sk-ant-session123"));

    // OAuth should return empty cookie header
    auto oauthRouting = ClaudeCredentialRouting::resolve(
        QStringLiteral("sk-ant-oat-test"),
        QString());

    QVERIFY(oauthRouting.cookieHeader().isEmpty());
}

void tst_ClaudeCredentialRouting::sourceLabel()
{
    auto oauthRouting = ClaudeCredentialRouting::resolve(
        QStringLiteral("sk-ant-oat-test"),
        QString());
    QCOMPARE(oauthRouting.sourceLabel(), QStringLiteral("oauth"));

    auto webRouting = ClaudeCredentialRouting::resolve(
        QString(),
        QStringLiteral("sk-ant-session"));
    QCOMPARE(webRouting.sourceLabel(), QStringLiteral("web"));

    auto emptyRouting = ClaudeCredentialRouting::resolve(QString(), QString());
    QVERIFY(emptyRouting.sourceLabel().isEmpty());
}

QTEST_MAIN(tst_ClaudeCredentialRouting)
#include "tst_ClaudeCredentialRouting.moc"
