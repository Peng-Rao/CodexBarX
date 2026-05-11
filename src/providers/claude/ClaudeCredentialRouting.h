#pragma once

#include <QString>
#include <optional>

class ClaudeCredentialRouting {
public:
    enum class Kind {
        None,
        OAuth,
        WebCookie
    };

    ClaudeCredentialRouting() = default;

    static ClaudeCredentialRouting resolve(
        const QString& oauthAccessToken,
        const QString& webCookieValue);

    static ClaudeCredentialRouting fromTokenAccountCredentials(
        const class TokenAccountCredentials& creds);

    Kind kind() const { return m_kind; }
    bool isValid() const { return m_kind != Kind::None; }

    QString oauthAccessToken() const { return m_kind == Kind::OAuth ? m_value : QString(); }
    QString webCookieValue() const { return m_kind == Kind::WebCookie ? m_value : QString(); }
    QString cookieHeader() const;

    QString sourceLabel() const;

private:
    Kind m_kind = Kind::None;
    QString m_value;
};
