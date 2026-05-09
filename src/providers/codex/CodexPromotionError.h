#pragma once

#include <QString>
#include <optional>

/**
 * @brief Error types for Codex account promotion operations.
 */
enum class CodexPromotionError {
    None,
    TargetManagedAccountNotFound,
    TargetManagedAccountAuthMissing,
    TargetManagedAccountAuthUnreadable,
    LiveAccountUnreadable,
    LiveAccountMissingIdentityForPreservation,
    LiveAccountAPIKeyOnlyUnsupported,
    DisplacedLiveManagedAccountConflict,
    DisplacedLiveImportFailed,
    ManagedStoreCommitFailed,
    LiveAuthSwapFailed,
    AlreadyInProgress
};

/**
 * @brief User-facing error for Codex system account promotion.
 *
 * Contains a title and a user-friendly message suitable for display in UI.
 */
struct CodexSystemAccountPromotionUserFacingError {
    QString title;
    QString message;

    bool isEmpty() const { return title.isEmpty() && message.isEmpty(); }

    static CodexSystemAccountPromotionUserFacingError fromError(CodexPromotionError error);
    static CodexSystemAccountPromotionUserFacingError none() {
        return CodexSystemAccountPromotionUserFacingError{};
    }
};

/**
 * @brief Utility class for mapping promotion errors to user-facing messages.
 */
class CodexPromotionErrorMapper {
public:
    static CodexSystemAccountPromotionUserFacingError map(CodexPromotionError error);
    static QString titleForError(CodexPromotionError error);
    static QString messageForError(CodexPromotionError error);
};
