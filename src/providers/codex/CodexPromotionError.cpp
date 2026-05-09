#include "CodexPromotionError.h"
#include <QCoreApplication>

CodexSystemAccountPromotionUserFacingError CodexSystemAccountPromotionUserFacingError::fromError(CodexPromotionError error)
{
    return CodexPromotionErrorMapper::map(error);
}

CodexSystemAccountPromotionUserFacingError CodexPromotionErrorMapper::map(CodexPromotionError error)
{
    CodexSystemAccountPromotionUserFacingError result;
    result.title = titleForError(error);
    result.message = messageForError(error);
    return result;
}

QString CodexPromotionErrorMapper::titleForError(CodexPromotionError error)
{
    switch (error) {
        case CodexPromotionError::None:
            return QString();
        case CodexPromotionError::AlreadyInProgress:
            return QCoreApplication::translate("CodexPromotionError", "Operation in progress");
        default:
            return QCoreApplication::translate("CodexPromotionError", "Could not switch system account");
    }
}

QString CodexPromotionErrorMapper::messageForError(CodexPromotionError error)
{
    switch (error) {
        case CodexPromotionError::None:
            return QString();

        case CodexPromotionError::TargetManagedAccountNotFound:
            return QCoreApplication::translate("CodexPromotionError",
                "That account is no longer available in CodexBarX. "
                "Refresh the account list and try again.");

        case CodexPromotionError::TargetManagedAccountAuthMissing:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not find saved auth for that account. "
                "Re-authenticate it and try again.");

        case CodexPromotionError::TargetManagedAccountAuthUnreadable:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not read saved auth for that account. "
                "Re-authenticate it and try again.");

        case CodexPromotionError::LiveAccountUnreadable:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not read the current system account on this computer.");

        case CodexPromotionError::LiveAccountMissingIdentityForPreservation:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not safely preserve the current system account before switching.");

        case CodexPromotionError::LiveAccountAPIKeyOnlyUnsupported:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX can't replace a system account that is signed in with an API key only setup.");

        case CodexPromotionError::DisplacedLiveManagedAccountConflict:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX found another managed account that already uses the current system account. "
                "Resolve the duplicate account before switching.");

        case CodexPromotionError::DisplacedLiveImportFailed:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not save the current system account before switching.");

        case CodexPromotionError::ManagedStoreCommitFailed:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not update managed account storage.");

        case CodexPromotionError::LiveAuthSwapFailed:
            return QCoreApplication::translate("CodexPromotionError",
                "CodexBarX could not replace the live Codex auth on this computer.");

        case CodexPromotionError::AlreadyInProgress:
            return QCoreApplication::translate("CodexPromotionError",
                "Finish the current managed account change before switching the system account.");
    }

    return QCoreApplication::translate("CodexPromotionError", "An unknown error occurred.");
}
