#include "ClaudeCredentialRouting.h"
#include "../../account/TokenAccountCredentials.h"

ClaudeCredentialRouting ClaudeCredentialRouting::resolve(
    const QString& oauthAccessToken,
    const QString& webCookieValue)
{
    ClaudeCredentialRouting routing;

    if (!oauthAccessToken.isEmpty() && oauthAccessToken.startsWith(QStringLiteral("sk-ant-oat"))) {
        routing.m_kind = Kind::OAuth;
        routing.m_value = oauthAccessToken;
    } else if (!webCookieValue.isEmpty() && webCookieValue.startsWith(QStringLiteral("sk-ant-"))) {
        routing.m_kind = Kind::WebCookie;
        routing.m_value = webCookieValue;
    }

    return routing;
}

ClaudeCredentialRouting ClaudeCredentialRouting::fromTokenAccountCredentials(
    const TokenAccountCredentials& creds)
{
    ClaudeCredentialRouting routing;

    // Prefer OAuth over Web cookie
    if (creds.oauth.has_value() && creds.oauth->isValid()) {
        QString accessToken = creds.oauth->accessToken.toString();
        if (accessToken.startsWith(QStringLiteral("sk-ant-oat"))) {
            routing.m_kind = Kind::OAuth;
            routing.m_value = accessToken;
            return routing;
        }
    }

    if (creds.web.has_value() && creds.web->isValid()) {
        if (creds.web->cookieName == QStringLiteral("sessionKey")) {
            QString cookieValue = creds.web->cookieValue.toString();
            if (cookieValue.startsWith(QStringLiteral("sk-ant-"))) {
                routing.m_kind = Kind::WebCookie;
                routing.m_value = cookieValue;
            }
        }
    }

    return routing;
}

QString ClaudeCredentialRouting::cookieHeader() const
{
    if (m_kind == Kind::WebCookie && !m_value.isEmpty()) {
        return QStringLiteral("sessionKey=") + m_value;
    }
    return QString();
}

QString ClaudeCredentialRouting::sourceLabel() const
{
    switch (m_kind) {
    case Kind::OAuth:
        return QStringLiteral("oauth");
    case Kind::WebCookie:
        return QStringLiteral("web");
    default:
        return QString();
    }
}
