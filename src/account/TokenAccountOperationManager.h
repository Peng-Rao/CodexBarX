#pragma once

#include <QObject>
#include <QHash>
#include <QVariantMap>
#include <QString>
#include <functional>

class UsageBackend;

/**
 * @brief Manages token account operation lifecycle.
 *
 * Handles the creation, state tracking, and completion notification
 * for async token account operations (add, remove, set visibility, etc.).
 *
 * Extracted from UsageStore during Phase 4 refactoring.
 */
class TokenAccountOperationManager : public QObject {
    Q_OBJECT
public:
    explicit TokenAccountOperationManager(QObject* parent = nullptr);

    // Operation state access
    QVariantMap operationState() const;

    // Begin a new operation, returns operation ID
    QString beginOperation(const QString& kind,
                           const QString& providerId,
                           const QString& targetId);

    // Finish an operation
    void finishOperation(const QString& operationId,
                         const QString& providerId,
                         bool success,
                         const QString& message,
                         bool refreshProviderOnSuccess);

    // Synchronous account mutations used by legacy QML-facing API.
    QString addAccount(const QString& providerId,
                       const QString& displayName,
                       int sourceMode);

    QString addAccountWithApiKey(const QString& providerId,
                                 const QString& displayName,
                                 int sourceMode,
                                 const QString& apiKey);

    bool removeAccount(const QString& accountId);
    bool setVisibility(const QString& accountId, int visibility);
    bool setSourceMode(const QString& accountId, int sourceMode);
    bool setDefault(const QString& providerId, const QString& accountId);

    // Operation requests (async via backend) - returns operation ID
    QString requestAddAccount(const QString& providerId,
                              const QString& displayName,
                              int sourceMode,
                              UsageBackend* backend);

    QString requestAddAccountWithApiKey(const QString& providerId,
                                        const QString& displayName,
                                        int sourceMode,
                                        const QString& apiKey,
                                        UsageBackend* backend);

    QString requestRemoveAccount(const QString& accountId,
                                 UsageBackend* backend);

    QString requestSetVisibility(const QString& accountId,
                                 int visibility,
                                 UsageBackend* backend);

    QString requestSetSourceMode(const QString& accountId,
                                 int sourceMode,
                                 UsageBackend* backend);

    QString requestSetDefault(const QString& providerId,
                              const QString& accountId,
                              UsageBackend* backend);

    // Handle backend result for tokenAccount.* kinds
    void handleBackendResult(const QString& kind,
                             const QVariantMap& payload,
                             bool backendSuccess,
                             const QString& backendMessage);

signals:
    void operationStateChanged();
    void operationFinished(const QString& operationId,
                           const QString& providerId,
                           bool success,
                           const QString& message);
    void refreshProviderRequested(const QString& providerId);

private:
    void rebuildState();

    QHash<QString, QVariantMap> m_operations;
    QVariantMap m_state;
};
