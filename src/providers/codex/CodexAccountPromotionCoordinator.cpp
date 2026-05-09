#include "CodexAccountPromotionCoordinator.h"

#include <QDebug>

CodexAccountPromotionCoordinator::CodexAccountPromotionCoordinator(QObject* parent)
    : QObject(parent)
{
}

CodexAccountPromotionCoordinator::~CodexAccountPromotionCoordinator() = default;

void CodexAccountPromotionCoordinator::setPromotionService(CodexAccountPromotionService* service)
{
    if (m_promotionService) {
        disconnect(m_promotionService, nullptr, this, nullptr);
    }

    m_promotionService = service;

    if (m_promotionService) {
        connect(m_promotionService, &CodexAccountPromotionService::promotionStarted,
                this, &CodexAccountPromotionCoordinator::onPromotionStarted);
        connect(m_promotionService, &CodexAccountPromotionService::promotionFinished,
                this, &CodexAccountPromotionCoordinator::onPromotionFinished);
    }
}

bool CodexAccountPromotionCoordinator::isInteractionBlocked() const
{
    if (m_isPromoting) {
        return true;
    }

    if (m_isAuthenticatingLiveAccount) {
        return true;
    }

    return false;
}

void CodexAccountPromotionCoordinator::promoteAsync(const QString& managedAccountId)
{
    // Clear any previous error
    clearError();

    // Check if interaction is blocked
    if (isInteractionBlocked()) {
        setPromotionError(CodexPromotionError::AlreadyInProgress);
        CodexAccountPromotionResult result;
        result.outcome = PromotionOutcome::Failed;
        result.targetManagedAccountId = managedAccountId;
        result.errorMessage = tr("Finish the current managed account change before switching the system account.");
        emit promotionFinished(managedAccountId, result);
        return;
    }

    // Check if promotion service is available
    if (!m_promotionService) {
        setPromotionError(CodexPromotionError::None);
        CodexAccountPromotionResult result;
        result.outcome = PromotionOutcome::Failed;
        result.targetManagedAccountId = managedAccountId;
        result.errorMessage = tr("Promotion service not available");
        emit promotionFinished(managedAccountId, result);
        return;
    }

    // Delegate to promotion service
    m_promotionService->promoteAsync(managedAccountId);
}

void CodexAccountPromotionCoordinator::clearError()
{
    m_userFacingError = CodexSystemAccountPromotionUserFacingError::none();
    emit userFacingErrorChanged();
}

void CodexAccountPromotionCoordinator::setAuthenticatingLiveAccount(bool inProgress)
{
    if (m_isAuthenticatingLiveAccount != inProgress) {
        m_isAuthenticatingLiveAccount = inProgress;
        emit authenticatingLiveAccountChanged(inProgress);
        emit interactionBlockedChanged(isInteractionBlocked());
    }
}

void CodexAccountPromotionCoordinator::onPromotionStarted(const QString& accountId)
{
    setPromoting(true);
    m_promotingAccountId = accountId;
    emit promotionStarted(accountId);
}

void CodexAccountPromotionCoordinator::onPromotionFinished(const QString& accountId, const CodexAccountPromotionResult& result)
{
    // Map result to user-facing error if failed
    if (result.outcome == PromotionOutcome::Failed) {
        CodexPromotionError error = mapPromotionResultToError(result);
        setPromotionError(error);
    } else {
        // Clear any previous error on success
        clearError();
    }

    setPromoting(false);
    m_promotingAccountId.clear();
    emit promotionFinished(accountId, result);
}

void CodexAccountPromotionCoordinator::setPromoting(bool promoting)
{
    if (m_isPromoting != promoting) {
        m_isPromoting = promoting;
        emit promotingChanged(promoting);
        emit interactionBlockedChanged(isInteractionBlocked());
    }
}

void CodexAccountPromotionCoordinator::setUserFacingError(const CodexSystemAccountPromotionUserFacingError& error)
{
    m_userFacingError = error;
    emit userFacingErrorChanged();
}

void CodexAccountPromotionCoordinator::setPromotionError(CodexPromotionError error)
{
    setUserFacingError(CodexPromotionErrorMapper::map(error));
}

CodexPromotionError CodexAccountPromotionCoordinator::mapPromotionResultToError(const CodexAccountPromotionResult& result)
{
    // Parse error message to determine specific error type
    const QString& msg = result.errorMessage;

    if (msg.contains(QStringLiteral("not found"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("no longer available"), Qt::CaseInsensitive)) {
        return CodexPromotionError::TargetManagedAccountNotFound;
    }

    if (msg.contains(QStringLiteral("auth missing"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("could not find saved auth"), Qt::CaseInsensitive)) {
        return CodexPromotionError::TargetManagedAccountAuthMissing;
    }

    if (msg.contains(QStringLiteral("auth unreadable"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("could not read saved auth"), Qt::CaseInsensitive)) {
        return CodexPromotionError::TargetManagedAccountAuthUnreadable;
    }

    if (msg.contains(QStringLiteral("system account unreadable"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("could not read the current system account"), Qt::CaseInsensitive)) {
        return CodexPromotionError::LiveAccountUnreadable;
    }

    if (msg.contains(QStringLiteral("identity"), Qt::CaseInsensitive) &&
        msg.contains(QStringLiteral("preservation"), Qt::CaseInsensitive)) {
        return CodexPromotionError::LiveAccountMissingIdentityForPreservation;
    }

    if (msg.contains(QStringLiteral("api key"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("api-only"), Qt::CaseInsensitive)) {
        return CodexPromotionError::LiveAccountAPIKeyOnlyUnsupported;
    }

    if (msg.contains(QStringLiteral("conflict"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("duplicate"), Qt::CaseInsensitive)) {
        return CodexPromotionError::DisplacedLiveManagedAccountConflict;
    }

    if (msg.contains(QStringLiteral("import failed"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("could not save"), Qt::CaseInsensitive)) {
        return CodexPromotionError::DisplacedLiveImportFailed;
    }

    if (msg.contains(QStringLiteral("storage"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("managed account storage"), Qt::CaseInsensitive)) {
        return CodexPromotionError::ManagedStoreCommitFailed;
    }

    if (msg.contains(QStringLiteral("swap"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("replace"), Qt::CaseInsensitive) ||
        msg.contains(QStringLiteral("live auth"), Qt::CaseInsensitive)) {
        return CodexPromotionError::LiveAuthSwapFailed;
    }

    // Default to live auth swap failed for generic failures
    return CodexPromotionError::LiveAuthSwapFailed;
}
