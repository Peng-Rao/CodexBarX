#include "CodexDisplacedLivePreservation.h"
#include "../../models/CodexUsageResponse.h"
#include "CodexHomeScope.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// ============================================================================
// CodexDisplacedLivePreservationPlan
// ============================================================================

CodexDisplacedLivePreservationPlan::CodexDisplacedLivePreservationPlan(Kind kind)
    : m_kind(kind)
{
}

std::optional<PreservationNoneReason> CodexDisplacedLivePreservationPlan::noneReason() const
{
    return m_noneReason;
}

std::optional<PreservationRejectReason> CodexDisplacedLivePreservationPlan::rejectReason() const
{
    return m_rejectReason;
}

std::optional<PreservationImportReason> CodexDisplacedLivePreservationPlan::importReason() const
{
    return m_importReason;
}

std::optional<PreservationRefreshReason> CodexDisplacedLivePreservationPlan::refreshReason() const
{
    return m_refreshReason;
}

std::optional<PreservationRepairReason> CodexDisplacedLivePreservationPlan::repairReason() const
{
    return m_repairReason;
}

CodexDisplacedLivePreservationPlan CodexDisplacedLivePreservationPlan::none(PreservationNoneReason reason)
{
    CodexDisplacedLivePreservationPlan plan(Kind::None);
    plan.m_noneReason = reason;
    return plan;
}

CodexDisplacedLivePreservationPlan CodexDisplacedLivePreservationPlan::reject(PreservationRejectReason reason)
{
    CodexDisplacedLivePreservationPlan plan(Kind::Reject);
    plan.m_rejectReason = reason;
    return plan;
}

CodexDisplacedLivePreservationPlan CodexDisplacedLivePreservationPlan::importNew(const PreservationImportReason& reason)
{
    CodexDisplacedLivePreservationPlan plan(Kind::ImportNew);
    plan.m_importReason = reason;
    return plan;
}

CodexDisplacedLivePreservationPlan CodexDisplacedLivePreservationPlan::refreshExisting(const PreservationRefreshReason& reason)
{
    CodexDisplacedLivePreservationPlan plan(Kind::RefreshExisting);
    plan.m_refreshReason = reason;
    return plan;
}

CodexDisplacedLivePreservationPlan CodexDisplacedLivePreservationPlan::repairExisting(const PreservationRepairReason& reason)
{
    CodexDisplacedLivePreservationPlan plan(Kind::RepairExisting);
    plan.m_repairReason = reason;
    return plan;
}

// ============================================================================
// CodexDisplacedLivePreservationPlanner
// ============================================================================

CodexDisplacedLivePreservationPlan CodexDisplacedLivePreservationPlanner::makePlan(
    const std::optional<ObservedSystemCodexAccount>& liveAccount,
    const CodexIdentity& targetIdentity,
    const QString& targetHomePath,
    const QHash<QString, CodexIdentity>& existingAccountIdentities,
    const QHash<QString, QString>& existingAccountHomePaths) const
{
    // Case 1: No live system account exists
    if (!liveAccount.has_value()) {
        return CodexDisplacedLivePreservationPlan::none(PreservationNoneReason::LiveIsMissing);
    }

    // Case 2: Live account is the same as target (promoting the live account itself)
    if (liveAccount->identity == targetIdentity) {
        return CodexDisplacedLivePreservationPlan::none(PreservationNoneReason::LiveMatchesTarget);
    }

    // Case 3: Target is already the live account (no displacement needed)
    if (targetHomePath.isEmpty()) {
        return CodexDisplacedLivePreservationPlan::none(PreservationNoneReason::TargetIsLive);
    }

    // Case 4: Live account has unresolved identity - cannot preserve
    if (liveAccount->identity.type() == CodexIdentityType::Unresolved) {
        return CodexDisplacedLivePreservationPlan::reject(PreservationRejectReason::LiveHasNoIdentity);
    }

    // Case 5: Check if live account credentials are readable
    QString liveHomePath = liveAccount->codexHomePath;
    QHash<QString, QString> scopedEnv = liveHomePath.isEmpty()
        ? QHash<QString, QString>()
        : CodexHomeScope::scopedEnvironment(existingAccountIdentities.empty() ? QHash<QString, QString>() : QHash<QString, QString>(), liveHomePath);

    auto credentials = CodexOAuthCredentials::load(scopedEnv);
    if (!credentials.has_value()) {
        return CodexDisplacedLivePreservationPlan::reject(PreservationRejectReason::LiveIsUnreadable);
    }

    // Case 6: API-only credentials (no OAuth tokens)
    if (credentials->accessToken.isEmpty() && credentials->refreshToken.isEmpty()) {
        return CodexDisplacedLivePreservationPlan::reject(PreservationRejectReason::LiveIsApiOnly);
    }

    // Case 7: Check if the live account matches an existing managed account
    std::optional<QString> matchingAccountIdOpt = findMatchingAccountId(existingAccountIdentities, liveAccount->identity);
    QString liveEmail = CodexIdentity::normalizeEmail(liveAccount->email);

    if (matchingAccountIdOpt.has_value()) {
        QString matchingAccountId = matchingAccountIdOpt.value();
        QString existingHomePath = existingAccountHomePaths.value(matchingAccountId);

        // Case 7a: Existing account has valid home path
        if (!existingHomePath.isEmpty()) {
            bool hadStale = credentials->refreshToken.isEmpty();
            return CodexDisplacedLivePreservationPlan::refreshExisting(
                PreservationRefreshReason{matchingAccountId, hadStale}
            );
        }

        // Case 7b: Existing account has missing home path - repair needed
        return CodexDisplacedLivePreservationPlan::repairExisting(
            PreservationRepairReason{matchingAccountId, existingHomePath}
        );
    }

    // Case 8: Import as new managed account
    PreservationImportReason importReason;
    importReason.email = liveEmail;
    if (liveAccount->identity.type() == CodexIdentityType::ProviderAccount) {
        importReason.providerAccountId = liveAccount->identity.accountId();
    }

    return CodexDisplacedLivePreservationPlan::importNew(importReason);
}

std::optional<QString> CodexDisplacedLivePreservationPlanner::findMatchingAccountId(
    const QHash<QString, CodexIdentity>& identities,
    const CodexIdentity& target) const
{
    for (auto it = identities.constBegin(); it != identities.constEnd(); ++it) {
        if (CodexIdentityMatcher::matches(it.value(), target)) {
            return it.key();
        }
    }
    return std::nullopt;
}

// ============================================================================
// CodexDisplacedLivePreservationExecutor
// ============================================================================

PreservationExecutionResult CodexDisplacedLivePreservationExecutor::execute(
    const CodexDisplacedLivePreservationPlan& plan,
    const DisplacedLivePreservationContext& context)
{
    PreservationExecutionResult result;

    switch (plan.kind()) {
    case CodexDisplacedLivePreservationPlan::Kind::None:
        result.success = true;
        result.disposition = QStringLiteral("none");
        return result;

    case CodexDisplacedLivePreservationPlan::Kind::Reject:
        result.success = true;
        result.disposition = QStringLiteral("rejected");
        return result;

    case CodexDisplacedLivePreservationPlan::Kind::ImportNew:
        return executeImportNew(plan.importReason().value(), context);

    case CodexDisplacedLivePreservationPlan::Kind::RefreshExisting:
        return executeRefreshExisting(plan.refreshReason().value(), context);

    case CodexDisplacedLivePreservationPlan::Kind::RepairExisting:
        return executeRepairExisting(plan.repairReason().value(), context);
    }

    result.success = false;
    result.errorMessage = QStringLiteral("Unknown plan kind");
    return result;
}

PreservationExecutionResult CodexDisplacedLivePreservationExecutor::executeImportNew(
    const PreservationImportReason& reason,
    const DisplacedLivePreservationContext& context)
{
    PreservationExecutionResult result;

    // Generate new account ID
    QString newAccountId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // Create managed home path for new account
    QString newHomePath = context.targetHomePath;
    if (newHomePath.isEmpty()) {
        newHomePath = QDir::homePath() + QStringLiteral("/.codex-managed-") + newAccountId.left(8);
    }

    // Ensure directory exists
    QDir dir(newHomePath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        result.success = false;
        result.errorMessage = QStringLiteral("Failed to create managed home directory: ") + newHomePath;
        return result;
    }

    // Atomically swap auth files from live location to new managed location
    QString errorMsg;
    if (!atomicSwapAuthFiles(context.liveCodexHomePath, newHomePath, errorMsg)) {
        result.success = false;
        result.errorMessage = errorMsg;
        return result;
    }

    result.success = true;
    result.createdAccountId = newAccountId;
    result.disposition = QStringLiteral("imported");
    return result;
}

PreservationExecutionResult CodexDisplacedLivePreservationExecutor::executeRefreshExisting(
    const PreservationRefreshReason& reason,
    const DisplacedLivePreservationContext& context)
{
    PreservationExecutionResult result;

    QString existingHomePath = context.existingAccountHomePaths.value(reason.existingAccountId);
    if (existingHomePath.isEmpty()) {
        result.success = false;
        result.errorMessage = QStringLiteral("No home path for existing account: ") + reason.existingAccountId;
        return result;
    }

    // Atomically swap auth files from live location to existing managed location
    QString errorMsg;
    if (!atomicSwapAuthFiles(context.liveCodexHomePath, existingHomePath, errorMsg)) {
        result.success = false;
        result.errorMessage = errorMsg;
        return result;
    }

    result.success = true;
    result.createdAccountId = reason.existingAccountId;
    result.disposition = QStringLiteral("refreshed");
    return result;
}

PreservationExecutionResult CodexDisplacedLivePreservationExecutor::executeRepairExisting(
    const PreservationRepairReason& reason,
    const DisplacedLivePreservationContext& context)
{
    PreservationExecutionResult result;

    // For repair, we need to create the missing home path and swap auth files
    QString newHomePath = context.targetHomePath;
    if (newHomePath.isEmpty()) {
        newHomePath = QDir::homePath() + QStringLiteral("/.codex-managed-") + reason.existingAccountId.left(8);
    }

    // Ensure directory exists
    QDir dir(newHomePath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        result.success = false;
        result.errorMessage = QStringLiteral("Failed to create repaired home directory: ") + newHomePath;
        return result;
    }

    // Atomically swap auth files from live location to repaired location
    QString errorMsg;
    if (!atomicSwapAuthFiles(context.liveCodexHomePath, newHomePath, errorMsg)) {
        result.success = false;
        result.errorMessage = errorMsg;
        return result;
    }

    result.success = true;
    result.createdAccountId = reason.existingAccountId;
    result.disposition = QStringLiteral("repaired");
    return result;
}

bool CodexDisplacedLivePreservationExecutor::atomicSwapAuthFiles(
    const QString& sourceHomePath,
    const QString& targetHomePath,
    QString& errorMessage)
{
    // Define auth file names
    const QStringList authFiles = {
        QStringLiteral("oauth_credentials.json"),
        QStringLiteral("auth.json"),
        QStringLiteral("token.json")
    };

    QString sourceAuthDir = sourceHomePath.isEmpty()
        ? QDir::homePath() + QStringLiteral("/.codex")
        : sourceHomePath;

    QString targetAuthDir = targetHomePath;

    // Ensure target directory exists
    QDir targetDir(targetAuthDir);
    if (!targetDir.exists() && !targetDir.mkpath(QStringLiteral("."))) {
        errorMessage = QStringLiteral("Failed to create target directory: ") + targetAuthDir;
        return false;
    }

#ifdef Q_OS_WIN
    // Windows: Use MoveFileEx with MOVEFILE_REPLACE_EXISTING for atomic move
    for (const QString& fileName : authFiles) {
        QString sourceFile = sourceAuthDir + QStringLiteral("/") + fileName;
        QString targetFile = targetAuthDir + QStringLiteral("/") + fileName;

        if (QFile::exists(sourceFile)) {
            // Ensure target file doesn't exist (MoveFileEx will replace, but let's be safe)
            if (QFile::exists(targetFile)) {
                QFile::remove(targetFile);
            }

            std::wstring sourcePathW = QDir::toNativeSeparators(sourceFile).toStdWString();
            std::wstring targetPathW = QDir::toNativeSeparators(targetFile).toStdWString();

            if (!MoveFileExW(sourcePathW.c_str(), targetPathW.c_str(),
                           MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                errorMessage = QStringLiteral("Failed to move auth file: ") + fileName;
                return false;
            }
        }
    }
#else
    // Unix: Use rename for atomic move
    for (const QString& fileName : authFiles) {
        QString sourceFile = sourceAuthDir + QStringLiteral("/") + fileName;
        QString targetFile = targetAuthDir + QStringLiteral("/") + fileName;

        if (QFile::exists(sourceFile)) {
            if (!QFile::rename(sourceFile, targetFile)) {
                errorMessage = QStringLiteral("Failed to move auth file: ") + fileName;
                return false;
            }
        }
    }
#endif

    return true;
}
