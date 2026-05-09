#pragma once

#include "CodexPromotionError.h"
#include "CodexAccountPromotionService.h"

#include <QObject>
#include <QPointer>

/**
 * @brief Coordinator for Codex account promotion operations.
 *
 * This class provides:
 * - Observable state for UI binding (isPromoting, isAuthenticatingLiveAccount)
 * - User-facing error handling with friendly messages
 * - Interaction blocking to prevent concurrent operations
 *
 * Similar to Swift's @Observable pattern, this class emits signals for state changes.
 */
class CodexAccountPromotionCoordinator : public QObject {
    Q_OBJECT

    // Observable properties for QML binding
    Q_PROPERTY(bool isPromoting READ isPromoting NOTIFY promotingChanged)
    Q_PROPERTY(bool isAuthenticatingLiveAccount READ isAuthenticatingLiveAccount NOTIFY authenticatingLiveAccountChanged)
    Q_PROPERTY(bool isInteractionBlocked READ isInteractionBlocked NOTIFY interactionBlockedChanged)
    Q_PROPERTY(QString userFacingErrorTitle READ userFacingErrorTitle NOTIFY userFacingErrorChanged)
    Q_PROPERTY(QString userFacingErrorMessage READ userFacingErrorMessage NOTIFY userFacingErrorChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY userFacingErrorChanged)

public:
    explicit CodexAccountPromotionCoordinator(QObject* parent = nullptr);
    ~CodexAccountPromotionCoordinator() override;

    // Dependency injection
    void setPromotionService(CodexAccountPromotionService* service);

    // State queries
    bool isPromoting() const { return m_isPromoting; }
    bool isAuthenticatingLiveAccount() const { return m_isAuthenticatingLiveAccount; }
    bool isInteractionBlocked() const;
    bool hasError() const { return !m_userFacingError.isEmpty(); }

    // Error access
    QString userFacingErrorTitle() const { return m_userFacingError.title; }
    QString userFacingErrorMessage() const { return m_userFacingError.message; }
    CodexSystemAccountPromotionUserFacingError userFacingError() const { return m_userFacingError; }

    // Operations
    void promoteAsync(const QString& managedAccountId);
    void clearError();

    // State setters (called by external components)
    void setAuthenticatingLiveAccount(bool inProgress);

signals:
    void promotingChanged(bool isPromoting);
    void authenticatingLiveAccountChanged(bool inProgress);
    void interactionBlockedChanged(bool blocked);
    void userFacingErrorChanged();
    void promotionStarted(const QString& accountId);
    void promotionFinished(const QString& accountId, const CodexAccountPromotionResult& result);

private slots:
    void onPromotionStarted(const QString& accountId);
    void onPromotionFinished(const QString& accountId, const CodexAccountPromotionResult& result);

private:
    void setPromoting(bool promoting);
    void setUserFacingError(const CodexSystemAccountPromotionUserFacingError& error);
    void setPromotionError(CodexPromotionError error);
    CodexPromotionError mapPromotionResultToError(const CodexAccountPromotionResult& result);

    QPointer<CodexAccountPromotionService> m_promotionService;

    bool m_isPromoting = false;
    bool m_isAuthenticatingLiveAccount = false;
    QString m_promotingAccountId;
    CodexSystemAccountPromotionUserFacingError m_userFacingError;
};
