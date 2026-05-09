#pragma once

#include "../../models/CodexUsageResponse.h"

#include <QString>
#include <QHash>
#include <QVector>
#include <optional>

struct CodexWorkspaceIdentity {
    QString workspaceId;
    QString workspaceName;
    QString workspaceAccountId;
    bool isDefault = false;

    bool operator==(const CodexWorkspaceIdentity& other) const;
    bool operator!=(const CodexWorkspaceIdentity& other) const;
};

struct CodexOpenAIWorkspaceIdentity {
    QString workspaceAccountID;
    QString workspaceLabel;

    bool operator==(const CodexOpenAIWorkspaceIdentity& other) const;
    bool operator!=(const CodexOpenAIWorkspaceIdentity& other) const;

    static QString normalizeWorkspaceAccountID(const QString& value);
    static QString normalizeWorkspaceLabel(const QString& value);
};

struct CodexWorkspaceResolveResult {
    bool success = false;
    QString errorMessage;
    QString currentWorkspaceAccountId;
    QString currentWorkspaceLabel;
    QVector<CodexWorkspaceIdentity> allWorkspaces;
};

class CodexOpenAIWorkspaceResolver {
public:
    // Legacy single-workspace resolve (synchronous)
    static std::optional<CodexOpenAIWorkspaceIdentity> resolve(
        const CodexOAuthCredentials& credentials,
        const QHash<QString, QString>& env);

    // JWT-based workspace extraction (can be called in background)
    static CodexWorkspaceResolveResult resolveFromJWT(const QString& idToken);
    static CodexWorkspaceResolveResult resolveFromCredentials(
        const CodexOAuthCredentials& credentials);

    // Async workspace resolution (requires backend)
    // Returns a job ID for tracking
    static QString resolveWorkspacesAsync(
        const QString& accessToken,
        const QString& idToken,
        const QHash<QString, QString>& env,
        int generation,
        std::function<void(const CodexWorkspaceResolveResult&)> callback);

    static QString normalizeWorkspaceAccountID(const QString& value);

private:
    static const QString AccountsURL;

    struct AccountItem {
        QString id;
        QString name;
    };

    static QString resolveWorkspaceLabel(const AccountItem& account);
    static QVector<CodexWorkspaceIdentity> parseWorkspacesFromJWT(const QJsonObject& payload);
};
