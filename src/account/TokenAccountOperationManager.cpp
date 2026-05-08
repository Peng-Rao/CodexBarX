#include "TokenAccountOperationManager.h"
#include "TokenAccountStore.h"
#include "../app/UsageBackend.h"

#include <QUuid>
#include <QDateTime>

TokenAccountOperationManager::TokenAccountOperationManager(QObject* parent)
    : QObject(parent)
{
}

QVariantMap TokenAccountOperationManager::operationState() const
{
    return m_state;
}

QString TokenAccountOperationManager::beginOperation(const QString& kind,
                                                      const QString& providerId,
                                                      const QString& targetId)
{
    const QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVariantMap op;
    op[QStringLiteral("operationId")] = operationId;
    op[QStringLiteral("kind")] = kind;
    op[QStringLiteral("providerId")] = providerId;
    op[QStringLiteral("targetId")] = targetId;
    op[QStringLiteral("state")] = QStringLiteral("pending");
    op[QStringLiteral("message")] = QString();
    op[QStringLiteral("startedAt")] = QDateTime::currentMSecsSinceEpoch();
    op[QStringLiteral("finishedAt")] = 0;
    m_operations.insert(operationId, op);
    rebuildState();
    emit operationStateChanged();
    return operationId;
}

void TokenAccountOperationManager::finishOperation(const QString& operationId,
                                                    const QString& providerId,
                                                    bool success,
                                                    const QString& message,
                                                    bool refreshProviderOnSuccess)
{
    auto op = m_operations.value(operationId);
    if (op.isEmpty()) {
        return;
    }
    op[QStringLiteral("state")] = success ? QStringLiteral("success") : QStringLiteral("error");
    op[QStringLiteral("message")] = message;
    op[QStringLiteral("finishedAt")] = QDateTime::currentMSecsSinceEpoch();
    m_operations.insert(operationId, op);
    rebuildState();
    emit operationStateChanged();
    emit operationFinished(operationId, providerId, success, message);
    if (success && refreshProviderOnSuccess && !providerId.isEmpty()) {
        emit refreshProviderRequested(providerId);
    }
}

void TokenAccountOperationManager::rebuildState()
{
    QVariantMap pendingByProvider;
    QVariantMap pendingByAccount;
    QVariantMap operations;
    int pendingCount = 0;

    for (auto it = m_operations.constBegin(); it != m_operations.constEnd(); ++it) {
        const QVariantMap op = it.value();
        operations.insert(it.key(), op);
        if (op.value(QStringLiteral("state")).toString() == QLatin1String("pending")) {
            ++pendingCount;
            const QString providerId = op.value(QStringLiteral("providerId")).toString();
            const QString targetId = op.value(QStringLiteral("targetId")).toString();
            if (!providerId.isEmpty()) {
                pendingByProvider.insert(providerId, true);
            }
            if (!targetId.isEmpty()) {
                pendingByAccount.insert(targetId, true);
            }
        }
    }

    m_state = {
        {QStringLiteral("pendingCount"), pendingCount},
        {QStringLiteral("pendingByProvider"), pendingByProvider},
        {QStringLiteral("pendingByAccount"), pendingByAccount},
        {QStringLiteral("operations"), operations}
    };
}

void TokenAccountOperationManager::handleBackendResult(const QString& kind,
                                                        const QVariantMap& payload,
                                                        bool backendSuccess,
                                                        const QString& backendMessage)
{
    const QString operationId = payload.value(QStringLiteral("operationId")).toString();
    const QString providerId = payload.value(QStringLiteral("providerId")).toString();
    const bool success = backendSuccess && payload.value(QStringLiteral("success"), true).toBool();
    const QString message = backendSuccess
        ? payload.value(QStringLiteral("message")).toString()
        : backendMessage;
    const bool refreshProviderOnSuccess = payload.value(QStringLiteral("refreshProviderOnSuccess")).toBool();

    finishOperation(operationId, providerId, success, message, refreshProviderOnSuccess);
}

QString TokenAccountOperationManager::requestAddAccount(const QString& providerId,
                                                         const QString& displayName,
                                                         int sourceMode,
                                                         UsageBackend* backend)
{
    TokenAccount account;
    account.accountId = TokenAccount::generateAccountId();
    account.providerId = providerId;
    account.displayName = displayName.trimmed().isEmpty() ? providerId : displayName.trimmed();
    account.sourceMode = static_cast<ProviderSourceMode>(sourceMode);
    account.visibility = AccountVisibility::Visible;
    account.createdAt = QDateTime::currentDateTimeUtc();
    account.lastUsedAt = account.createdAt;

    const QString operationId = beginOperation(
        QStringLiteral("addTokenAccount"), providerId, account.accountId);

    backend->dispatchValueJob(QStringLiteral("tokenAccount.add"), 0,
        [operationId, providerId, account]() -> QVariant {
            TokenAccountStore::instance()->addAccountMetadata(account);
            if (TokenAccountStore::instance()->accountCountForProvider(providerId) == 1) {
                TokenAccountStore::instance()->setDefaultAccountId(providerId, account.accountId);
            }
            const bool saved = TokenAccountStore::instance()->saveToDisk();
            QVariantMap payload;
            payload.insert(QStringLiteral("operationId"), operationId);
            payload.insert(QStringLiteral("providerId"), providerId);
            payload.insert(QStringLiteral("success"), saved);
            payload.insert(QStringLiteral("message"),
                           saved ? QString()
                                 : QStringLiteral("Failed to save token account configuration."));
            payload.insert(QStringLiteral("refreshProviderOnSuccess"), true);
            return payload;
        });

    return operationId;
}

QString TokenAccountOperationManager::requestAddAccountWithApiKey(const QString& providerId,
                                                                   const QString& displayName,
                                                                   int sourceMode,
                                                                   const QString& apiKey,
                                                                   UsageBackend* backend)
{
    TokenAccount account;
    account.accountId = TokenAccount::generateAccountId();
    account.providerId = providerId;
    account.displayName = displayName.trimmed().isEmpty() ? providerId : displayName.trimmed();
    account.sourceMode = static_cast<ProviderSourceMode>(sourceMode);
    account.visibility = AccountVisibility::Visible;
    account.createdAt = QDateTime::currentDateTimeUtc();
    account.lastUsedAt = account.createdAt;

    const QString trimmedKey = apiKey.trimmed();
    if (!trimmedKey.isEmpty()) {
        APICredentials api;
        api.apiKey = SecureString(trimmedKey);
        account.credentials.api = api;
    }

    const QString operationId = beginOperation(
        QStringLiteral("addTokenAccountWithApiKey"), providerId, account.accountId);

    TokenAccount metadata = account;
    metadata.credentials = {};
    TokenAccountCredentials credentials = account.credentials;

    backend->dispatchValueJob(QStringLiteral("tokenAccount.addApiKey"), 0,
        [operationId, providerId, metadata, credentials]() -> QVariant {
            bool ok = true;
            QString message;
            if (!credentials.isEmpty()) {
                ok = TokenAccountStore::instance()->saveAccountCredentials(metadata.accountId, credentials);
                if (!ok) {
                    message = QStringLiteral("Failed to save token account credentials.");
                }
            }
            if (ok) {
                TokenAccountStore::instance()->addAccountMetadata(metadata);
                if (TokenAccountStore::instance()->accountCountForProvider(providerId) == 1) {
                    TokenAccountStore::instance()->setDefaultAccountId(providerId, metadata.accountId);
                }
                ok = TokenAccountStore::instance()->saveToDisk();
                if (!ok) {
                    message = QStringLiteral("Failed to save token account configuration.");
                }
            }
            QVariantMap payload;
            payload.insert(QStringLiteral("operationId"), operationId);
            payload.insert(QStringLiteral("providerId"), providerId);
            payload.insert(QStringLiteral("success"), ok);
            payload.insert(QStringLiteral("message"), message);
            payload.insert(QStringLiteral("refreshProviderOnSuccess"), true);
            return payload;
        });

    return operationId;
}

QString TokenAccountOperationManager::requestRemoveAccount(const QString& accountId,
                                                            UsageBackend* backend)
{
    auto accOpt = TokenAccountStore::instance()->accountMetadata(accountId);
    if (!accOpt.has_value()) {
        return QString();
    }
    const QString providerId = accOpt->providerId;
    const QString operationId = beginOperation(
        QStringLiteral("removeTokenAccount"), providerId, accountId);

    backend->dispatchValueJob(QStringLiteral("tokenAccount.remove"), 0,
        [operationId, providerId, accountId]() -> QVariant {
            TokenAccountStore::instance()->removeAccountCredentials(accountId);
            bool ok = TokenAccountStore::instance()->removeAccountMetadata(accountId);
            QString message;
            if (!ok) {
                message = QStringLiteral("Token account no longer exists.");
            } else {
                ok = TokenAccountStore::instance()->saveToDisk();
                if (!ok) {
                    message = QStringLiteral("Failed to save token account configuration.");
                }
            }
            QVariantMap payload;
            payload.insert(QStringLiteral("operationId"), operationId);
            payload.insert(QStringLiteral("providerId"), providerId);
            payload.insert(QStringLiteral("success"), ok);
            payload.insert(QStringLiteral("message"), message);
            payload.insert(QStringLiteral("refreshProviderOnSuccess"), true);
            return payload;
        });

    return operationId;
}

QString TokenAccountOperationManager::requestSetVisibility(const QString& accountId,
                                                            int visibility,
                                                            UsageBackend* backend)
{
    auto accOpt = TokenAccountStore::instance()->accountMetadata(accountId);
    if (!accOpt.has_value()) {
        return QString();
    }
    TokenAccount acc = accOpt.value();
    acc.visibility = static_cast<AccountVisibility>(visibility);
    const QString providerId = acc.providerId;
    const QString operationId = beginOperation(
        QStringLiteral("setTokenAccountVisibility"), providerId, accountId);

    backend->dispatchValueJob(QStringLiteral("tokenAccount.visibility"), 0,
        [operationId, providerId, accountId, acc]() -> QVariant {
            bool ok = TokenAccountStore::instance()->updateAccountMetadata(accountId, acc);
            QString message;
            if (!ok) {
                message = QStringLiteral("Token account no longer exists.");
            } else {
                ok = TokenAccountStore::instance()->saveToDisk();
                if (!ok) {
                    message = QStringLiteral("Failed to save token account configuration.");
                }
            }
            QVariantMap payload;
            payload.insert(QStringLiteral("operationId"), operationId);
            payload.insert(QStringLiteral("providerId"), providerId);
            payload.insert(QStringLiteral("success"), ok);
            payload.insert(QStringLiteral("message"), message);
            payload.insert(QStringLiteral("refreshProviderOnSuccess"), true);
            return payload;
        });

    return operationId;
}

QString TokenAccountOperationManager::requestSetSourceMode(const QString& accountId,
                                                            int sourceMode,
                                                            UsageBackend* backend)
{
    auto accOpt = TokenAccountStore::instance()->accountMetadata(accountId);
    if (!accOpt.has_value()) {
        return QString();
    }
    TokenAccount acc = accOpt.value();
    acc.sourceMode = static_cast<ProviderSourceMode>(sourceMode);
    const QString providerId = acc.providerId;
    const QString operationId = beginOperation(
        QStringLiteral("setTokenAccountSourceMode"), providerId, accountId);

    backend->dispatchValueJob(QStringLiteral("tokenAccount.sourceMode"), 0,
        [operationId, providerId, accountId, acc]() -> QVariant {
            bool ok = TokenAccountStore::instance()->updateAccountMetadata(accountId, acc);
            QString message;
            if (!ok) {
                message = QStringLiteral("Token account no longer exists.");
            } else {
                ok = TokenAccountStore::instance()->saveToDisk();
                if (!ok) {
                    message = QStringLiteral("Failed to save token account configuration.");
                }
            }
            QVariantMap payload;
            payload.insert(QStringLiteral("operationId"), operationId);
            payload.insert(QStringLiteral("providerId"), providerId);
            payload.insert(QStringLiteral("success"), ok);
            payload.insert(QStringLiteral("message"), message);
            payload.insert(QStringLiteral("refreshProviderOnSuccess"), true);
            return payload;
        });

    return operationId;
}

QString TokenAccountOperationManager::requestSetDefault(const QString& providerId,
                                                         const QString& accountId,
                                                         UsageBackend* backend)
{
    if (!accountId.isEmpty()) {
        auto accOpt = TokenAccountStore::instance()->accountMetadata(accountId);
        if (!accOpt.has_value() || accOpt->providerId != providerId) {
            return QString();
        }
    }

    const QString operationId = beginOperation(
        QStringLiteral("setDefaultTokenAccount"), providerId, accountId);

    backend->dispatchValueJob(QStringLiteral("tokenAccount.default"), 0,
        [operationId, providerId, accountId]() -> QVariant {
            TokenAccountStore::instance()->setDefaultAccountId(providerId, accountId);
            const bool saved = TokenAccountStore::instance()->saveToDisk();
            QVariantMap payload;
            payload.insert(QStringLiteral("operationId"), operationId);
            payload.insert(QStringLiteral("providerId"), providerId);
            payload.insert(QStringLiteral("success"), saved);
            payload.insert(QStringLiteral("message"),
                           saved ? QString()
                                 : QStringLiteral("Failed to save token account configuration."));
            payload.insert(QStringLiteral("refreshProviderOnSuccess"), true);
            return payload;
        });

    return operationId;
}
