#include "CodexOpenAIWorkspaceResolver.h"
#include "../../network/NetworkManager.h"
#include "../../models/CodexUsageResponse.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

const QString CodexOpenAIWorkspaceResolver::AccountsURL = "https://chatgpt.com/backend-api/accounts";

// ============================================================================
// CodexWorkspaceIdentity
// ============================================================================

bool CodexWorkspaceIdentity::operator==(const CodexWorkspaceIdentity& other) const
{
    return workspaceId == other.workspaceId &&
           workspaceName == other.workspaceName &&
           workspaceAccountId == other.workspaceAccountId &&
           isDefault == other.isDefault;
}

bool CodexWorkspaceIdentity::operator!=(const CodexWorkspaceIdentity& other) const
{
    return !(*this == other);
}

// ============================================================================
// CodexOpenAIWorkspaceIdentity
// ============================================================================

bool CodexOpenAIWorkspaceIdentity::operator==(const CodexOpenAIWorkspaceIdentity& other) const
{
    return workspaceAccountID == other.workspaceAccountID && workspaceLabel == other.workspaceLabel;
}

bool CodexOpenAIWorkspaceIdentity::operator!=(const CodexOpenAIWorkspaceIdentity& other) const
{
    return !(*this == other);
}

QString CodexOpenAIWorkspaceIdentity::normalizeWorkspaceAccountID(const QString& value)
{
    return value.trimmed().toLower();
}

QString CodexOpenAIWorkspaceIdentity::normalizeWorkspaceLabel(const QString& value)
{
    QString trimmed = value.trimmed();
    return trimmed.isEmpty() ? QString() : trimmed;
}

// ============================================================================
// CodexOpenAIWorkspaceResolver
// ============================================================================

std::optional<CodexOpenAIWorkspaceIdentity> CodexOpenAIWorkspaceResolver::resolve(
    const CodexOAuthCredentials& credentials,
    const QHash<QString, QString>& env)
{
    Q_UNUSED(env);

    QString workspaceAccountID = normalizeWorkspaceAccountID(credentials.accountId);
    if (workspaceAccountID.isEmpty()) {
        return std::nullopt;
    }

    QHash<QString, QString> headers;
    headers["Authorization"] = "Bearer " + credentials.accessToken;
    headers["User-Agent"] = "codex-cli";
    headers["Accept"] = "application/json";
    headers["ChatGPT-Account-Id"] = workspaceAccountID;

    QJsonObject json = NetworkManager::instance().getJsonSync(
        QUrl(AccountsURL), headers, 20000);

    if (json.isEmpty()) {
        return CodexOpenAIWorkspaceIdentity{workspaceAccountID, QString()};
    }

    QJsonArray items = json.value("items").toArray();
    for (const auto& item : items) {
        QJsonObject obj = item.toObject();
        QString id = obj.value("id").toString();
        if (normalizeWorkspaceAccountID(id) == workspaceAccountID) {
            AccountItem account;
            account.id = id;
            account.name = obj.value("name").toString();
            return CodexOpenAIWorkspaceIdentity{
                workspaceAccountID,
                resolveWorkspaceLabel(account)
            };
        }
    }

    return CodexOpenAIWorkspaceIdentity{workspaceAccountID, QString()};
}

CodexWorkspaceResolveResult CodexOpenAIWorkspaceResolver::resolveFromJWT(const QString& idToken)
{
    CodexWorkspaceResolveResult result;

    if (idToken.isEmpty()) {
        result.success = false;
        result.errorMessage = QStringLiteral("ID token is empty");
        return result;
    }

    QJsonObject payload = parseJWTPayload(idToken);
    if (payload.isEmpty()) {
        result.success = false;
        result.errorMessage = QStringLiteral("Failed to decode JWT payload");
        return result;
    }

    // Parse workspaces from JWT
    result.allWorkspaces = parseWorkspacesFromJWT(payload);

    // Get current account ID from JWT
    QString currentAccountId = payload.value(QStringLiteral("https://api.openai.com/account_id"))
                                   .toString();

    if (currentAccountId.isEmpty()) {
        currentAccountId = payload.value(QStringLiteral("sub")).toString();
    }

    if (!currentAccountId.isEmpty()) {
        result.currentWorkspaceAccountId = normalizeWorkspaceAccountID(currentAccountId);

        // Find matching workspace for label
        for (const auto& ws : result.allWorkspaces) {
            if (normalizeWorkspaceAccountID(ws.workspaceAccountId) == result.currentWorkspaceAccountId) {
                result.currentWorkspaceLabel = ws.workspaceName;
                break;
            }
        }
    }

    result.success = true;
    return result;
}

CodexWorkspaceResolveResult CodexOpenAIWorkspaceResolver::resolveFromCredentials(
    const CodexOAuthCredentials& credentials)
{
    CodexWorkspaceResolveResult result;

    // First try JWT-based resolution
    if (!credentials.idToken.isEmpty()) {
        result = resolveFromJWT(credentials.idToken);
        if (result.success) {
            return result;
        }
    }

    // Fallback to account ID from credentials
    QString accountId = normalizeWorkspaceAccountID(credentials.accountId);
    if (!accountId.isEmpty()) {
        result.success = true;
        result.currentWorkspaceAccountId = accountId;
        result.currentWorkspaceLabel = QStringLiteral("Personal");
    } else {
        result.success = false;
        result.errorMessage = QStringLiteral("No account ID available");
    }

    return result;
}

QString CodexOpenAIWorkspaceResolver::resolveWorkspacesAsync(
    const QString& accessToken,
    const QString& idToken,
    const QHash<QString, QString>& env,
    int generation,
    std::function<void(const CodexWorkspaceResolveResult&)> callback)
{
    Q_UNUSED(accessToken);
    Q_UNUSED(env);
    Q_UNUSED(generation);

    // For now, we do JWT-based resolution which is fast enough to be synchronous
    // The callback pattern allows future async implementation
    CodexWorkspaceResolveResult result = resolveFromJWT(idToken);

    if (callback) {
        callback(result);
    }

    return QStringLiteral("workspace-") + QString::number(generation);
}

QString CodexOpenAIWorkspaceResolver::normalizeWorkspaceAccountID(const QString& value)
{
    return CodexOpenAIWorkspaceIdentity::normalizeWorkspaceAccountID(value);
}

QString CodexOpenAIWorkspaceResolver::resolveWorkspaceLabel(const AccountItem& account)
{
    QString name = account.name.trimmed();
    if (!name.isEmpty()) return name;
    return "Personal";
}

QVector<CodexWorkspaceIdentity> CodexOpenAIWorkspaceResolver::parseWorkspacesFromJWT(const QJsonObject& payload)
{
    QVector<CodexWorkspaceIdentity> workspaces;

    // OpenAI JWT may contain workspaces in different locations
    // Try https://api.openai.com/workspaces first
    QJsonArray workspacesArray = payload.value(QStringLiteral("https://api.openai.com/workspaces"))
                                     .toArray();

    if (workspacesArray.isEmpty()) {
        // Try nested structure
        QJsonObject profile = payload.value(QStringLiteral("https://api.openai.com/profile")).toObject();
        workspacesArray = profile.value(QStringLiteral("workspaces")).toArray();
    }

    for (const auto& item : workspacesArray) {
        QJsonObject wsObj = item.toObject();
        CodexWorkspaceIdentity ws;
        ws.workspaceId = wsObj.value(QStringLiteral("id")).toString();
        ws.workspaceName = wsObj.value(QStringLiteral("name")).toString();
        ws.workspaceAccountId = wsObj.value(QStringLiteral("account_id")).toString();
        if (ws.workspaceAccountId.isEmpty()) {
            ws.workspaceAccountId = wsObj.value(QStringLiteral("accountId")).toString();
        }
        ws.isDefault = wsObj.value(QStringLiteral("is_default")).toBool();
        if (!ws.isDefault) {
            ws.isDefault = wsObj.value(QStringLiteral("isDefault")).toBool();
        }

        if (!ws.workspaceAccountId.isEmpty()) {
            workspaces.append(ws);
        }
    }

    // If no workspaces found, create a single entry from the current account
    if (workspaces.isEmpty()) {
        QString currentAccountId = payload.value(QStringLiteral("https://api.openai.com/account_id")).toString();
        if (currentAccountId.isEmpty()) {
            currentAccountId = payload.value(QStringLiteral("sub")).toString();
        }

        if (!currentAccountId.isEmpty()) {
            CodexWorkspaceIdentity ws;
            ws.workspaceAccountId = currentAccountId;
            ws.workspaceName = QStringLiteral("Personal");
            ws.isDefault = true;
            workspaces.append(ws);
        }
    }

    return workspaces;
}
