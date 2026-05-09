#include "CodexAccountPromotionService.h"
#include "../../app/UsageBackend.h"
#include "CodexHomeScope.h"
#include "ManagedCodexAccount.h"
#include "CodexSystemAccountObserver.h"
#include "CodexAtomicFileSwap.h"
#include "../../models/CodexUsageResponse.h"

#include <QDir>
#include <QFile>
#include <QUuid>
#include <QStandardPaths>
#include <QDebug>

CodexAccountPromotionService::CodexAccountPromotionService(QObject* parent)
    : QObject(parent)
{
}

CodexAccountPromotionService::~CodexAccountPromotionService() = default;

void CodexAccountPromotionService::setBackend(UsageBackend* backend)
{
    m_backend = backend;
}

void CodexAccountPromotionService::setEnvironment(const QHash<QString, QString>& env)
{
    m_env = env;
}

void CodexAccountPromotionService::promoteAsync(const QString& accountId)
{
    if (m_isPromoting) {
        CodexAccountPromotionResult result;
        result.outcome = PromotionOutcome::Failed;
        result.errorMessage = tr("Already promoting an account");
        emit promotionFinished(accountId, result);
        return;
    }

    m_isPromoting = true;
    m_promotingAccountId = accountId;
    emit promotionStarted(accountId);

    if (!m_backend) {
        // Synchronous fallback
        CodexAccountPromotionResult result = executePromotion(accountId);
        m_isPromoting = false;
        m_promotingAccountId.clear();
        emit promotionFinished(accountId, result);
        return;
    }

    // Async execution via backend
    QString capturedAccountId = accountId;
    QHash<QString, QString> capturedEnv = m_env;

    m_backend->dispatchValueJob(QStringLiteral("codexPromotion"), 0,
        [capturedAccountId, capturedEnv]() -> QVariant {
            ManagedCodexAccountStore store;
            CodexSystemAccountObserver observer;

            CodexAccountPromotionContext ctx = CodexAccountPromotionContext::build(
                capturedAccountId, capturedEnv, store, observer
            );

            if (!ctx.valid) {
                CodexAccountPromotionResult result;
                result.outcome = PromotionOutcome::Failed;
                result.targetManagedAccountId = capturedAccountId;
                result.errorMessage = ctx.errorMessage;
                return QVariant::fromValue(result);
            }

            // Check for converged identity (no-op case)
            if (ctx.liveAccount.has_value()) {
                CodexIdentity liveIdentity = ctx.liveAccount->identity;
                if (CodexIdentityMatcher::matches(ctx.targetIdentity, liveIdentity)) {
                    CodexAccountPromotionResult result;
                    result.outcome = PromotionOutcome::ConvergedNoOp;
                    result.targetManagedAccountId = capturedAccountId;
                    return QVariant::fromValue(result);
                }
            }

            // Create preservation plan
            CodexDisplacedLivePreservationPlanner planner;
            CodexDisplacedLivePreservationPlan preservationPlan = planner.makePlan(
                ctx.liveAccount,
                ctx.targetIdentity,
                ctx.targetHomePath,
                ctx.existingAccountIdentities,
                ctx.existingAccountHomePaths
            );

            // Execute preservation
            DisplacedLivePreservationContext execContext;
            execContext.targetHomePath = ctx.targetHomePath;
            execContext.liveCodexHomePath = ctx.liveHomePath;
            execContext.env = capturedEnv;
            execContext.existingAccountIdentities = ctx.existingAccountIdentities;
            execContext.existingAccountHomePaths = ctx.existingAccountHomePaths;

            CodexDisplacedLivePreservationExecutor executor;
            PreservationExecutionResult preservationResult = executor.execute(preservationPlan, execContext);

            if (!preservationResult.success) {
                CodexAccountPromotionResult result;
                result.outcome = PromotionOutcome::Failed;
                result.targetManagedAccountId = capturedAccountId;
                result.errorMessage = preservationResult.errorMessage;
                return QVariant::fromValue(result);
            }

            // Atomically swap auth files from managed to live location
            QString errorMsg;
            bool swapped = false;

            QString sourceAuthPath = ctx.targetHomePath.isEmpty()
                ? QString()
                : ctx.targetHomePath + QStringLiteral("/auth.json");

            QString targetAuthPath = ctx.liveHomePath + QStringLiteral("/auth.json");

            // Read managed auth content
            QByteArray managedAuthContent;
            {
                QFile f(sourceAuthPath);
                if (f.open(QIODevice::ReadOnly)) {
                    managedAuthContent = f.readAll();
                    f.close();
                }
            }

            if (managedAuthContent.isEmpty()) {
                CodexAccountPromotionResult result;
                result.outcome = PromotionOutcome::Failed;
                result.targetManagedAccountId = capturedAccountId;
                result.errorMessage = QStringLiteral("Failed to read managed auth file");
                return QVariant::fromValue(result);
            }

            // Atomically write to live location using staging
            CodexAtomicFileSwap swapper(targetAuthPath);
            if (!swapper.stageFile(managedAuthContent)) {
                CodexAccountPromotionResult result;
                result.outcome = PromotionOutcome::Failed;
                result.targetManagedAccountId = capturedAccountId;
                result.errorMessage = QStringLiteral("Failed to stage auth file: %1").arg(swapper.errorMessage());
                return QVariant::fromValue(result);
            }

            if (!swapper.commit()) {
                CodexAccountPromotionResult result;
                result.outcome = PromotionOutcome::Failed;
                result.targetManagedAccountId = capturedAccountId;
                result.errorMessage = QStringLiteral("Failed to commit auth file: %1").arg(swapper.errorMessage());
                return QVariant::fromValue(result);
            }

            swapped = true;

            // Build success result
            CodexAccountPromotionResult result;
            result.outcome = PromotionOutcome::Promoted;
            result.targetManagedAccountId = capturedAccountId;
            result.didMutateLiveAuth = true;
            result.resultingActiveSource = CodexActiveSource::LiveSystem;

            // Set displaced disposition based on preservation result
            if (preservationPlan.kind() == CodexDisplacedLivePreservationPlan::Kind::None) {
                result.displacedDisposition = DisplacedLiveDisposition::None;
            } else if (preservationPlan.kind() == CodexDisplacedLivePreservationPlan::Kind::Reject) {
                result.displacedDisposition = DisplacedLiveDisposition::Rejected;
            } else if (preservationPlan.kind() == CodexDisplacedLivePreservationPlan::Kind::ImportNew) {
                result.displacedDisposition = DisplacedLiveDisposition::Imported;
                result.displacedManagedAccountId = preservationResult.createdAccountId;
            } else if (preservationPlan.kind() == CodexDisplacedLivePreservationPlan::Kind::RefreshExisting ||
                       preservationPlan.kind() == CodexDisplacedLivePreservationPlan::Kind::RepairExisting) {
                result.displacedDisposition = DisplacedLiveDisposition::AlreadyManaged;
                result.displacedManagedAccountId = preservationResult.createdAccountId;
            }

            return QVariant::fromValue(result);
        });
}

CodexAccountPromotionResult CodexAccountPromotionService::executePromotion(const QString& accountId)
{
    CodexAccountPromotionResult result;
    result.targetManagedAccountId = accountId;

    ManagedCodexAccountStore store;
    CodexSystemAccountObserver observer;

    CodexAccountPromotionContext ctx = CodexAccountPromotionContext::build(
        accountId, m_env, store, observer
    );

    if (!ctx.valid) {
        result.outcome = PromotionOutcome::Failed;
        result.errorMessage = ctx.errorMessage;
        return result;
    }

    // Simplified synchronous promotion with atomic swap
    QString sourceAuthPath = ctx.targetHomePath + QStringLiteral("/auth.json");
    QString targetAuthPath = ctx.liveHomePath + QStringLiteral("/auth.json");

    QByteArray authContent;
    {
        QFile f(sourceAuthPath);
        if (!f.open(QIODevice::ReadOnly)) {
            result.outcome = PromotionOutcome::Failed;
            result.errorMessage = QStringLiteral("Cannot read managed auth");
            return result;
        }
        authContent = f.readAll();
    }

    // Use atomic file swap
    CodexAtomicFileSwap swapper(targetAuthPath);
    if (!swapper.stageFile(authContent)) {
        result.outcome = PromotionOutcome::Failed;
        result.errorMessage = QStringLiteral("Cannot stage auth: %1").arg(swapper.errorMessage());
        return result;
    }

    if (!swapper.commit()) {
        result.outcome = PromotionOutcome::Failed;
        result.errorMessage = QStringLiteral("Cannot commit auth: %1").arg(swapper.errorMessage());
        return result;
    }

    result.outcome = PromotionOutcome::Promoted;
    result.didMutateLiveAuth = true;
    result.resultingActiveSource = CodexActiveSource::LiveSystem;
    return result;
}
