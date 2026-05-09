#pragma once

#include "CodexIdentity.h"
#include "ManagedCodexAccount.h"
#include "CodexSystemAccountObserver.h"

#include <QString>
#include <QHash>
#include <optional>
#include <functional>

enum class PreservationNoneReason {
    LiveIsMissing,          // System account does not exist
    LiveMatchesTarget,      // System account matches target
    TargetIsLive            // Target is already live system
};

enum class PreservationRejectReason {
    LiveIsUnreadable,       // System account not readable
    LiveIsApiOnly,          // API Key only authentication
    LiveHasNoIdentity       // Cannot identify identity
};

struct PreservationImportReason {
    QString email;
    QString providerAccountId;
};

struct PreservationRefreshReason {
    QString existingAccountId;
    bool hadStaleCredentials = false;
};

struct PreservationRepairReason {
    QString existingAccountId;
    QString missingHomePath;
};

class CodexDisplacedLivePreservationPlan {
public:
    enum class Kind {
        None,
        Reject,
        ImportNew,
        RefreshExisting,
        RepairExisting
    };

    CodexDisplacedLivePreservationPlan() = default;

    Kind kind() const { return m_kind; }

    std::optional<PreservationNoneReason> noneReason() const;
    std::optional<PreservationRejectReason> rejectReason() const;
    std::optional<PreservationImportReason> importReason() const;
    std::optional<PreservationRefreshReason> refreshReason() const;
    std::optional<PreservationRepairReason> repairReason() const;

    static CodexDisplacedLivePreservationPlan none(PreservationNoneReason reason);
    static CodexDisplacedLivePreservationPlan reject(PreservationRejectReason reason);
    static CodexDisplacedLivePreservationPlan importNew(const PreservationImportReason& reason);
    static CodexDisplacedLivePreservationPlan refreshExisting(const PreservationRefreshReason& reason);
    static CodexDisplacedLivePreservationPlan repairExisting(const PreservationRepairReason& reason);

private:
    explicit CodexDisplacedLivePreservationPlan(Kind kind);

    Kind m_kind = Kind::None;
    std::optional<PreservationNoneReason> m_noneReason;
    std::optional<PreservationRejectReason> m_rejectReason;
    std::optional<PreservationImportReason> m_importReason;
    std::optional<PreservationRefreshReason> m_refreshReason;
    std::optional<PreservationRepairReason> m_repairReason;
};

struct DisplacedLivePreservationContext {
    QString targetHomePath;
    QString liveCodexHomePath;
    QHash<QString, QString> env;
    QHash<QString, CodexIdentity> existingAccountIdentities;
    QHash<QString, QString> existingAccountHomePaths;
};

class CodexDisplacedLivePreservationPlanner {
public:
    CodexDisplacedLivePreservationPlan makePlan(
        const std::optional<ObservedSystemCodexAccount>& liveAccount,
        const CodexIdentity& targetIdentity,
        const QString& targetHomePath,
        const QHash<QString, CodexIdentity>& existingAccountIdentities,
        const QHash<QString, QString>& existingAccountHomePaths
    ) const;

private:
    std::optional<QString> findMatchingAccountId(
        const QHash<QString, CodexIdentity>& identities,
        const CodexIdentity& target
    ) const;
};

struct PreservationExecutionResult {
    bool success = false;
    QString errorMessage;
    QString createdAccountId;
    QString disposition;
};

class CodexDisplacedLivePreservationExecutor {
public:
    PreservationExecutionResult execute(
        const CodexDisplacedLivePreservationPlan& plan,
        const DisplacedLivePreservationContext& context
    );

private:
    PreservationExecutionResult executeImportNew(
        const PreservationImportReason& reason,
        const DisplacedLivePreservationContext& context
    );

    PreservationExecutionResult executeRefreshExisting(
        const PreservationRefreshReason& reason,
        const DisplacedLivePreservationContext& context
    );

    PreservationExecutionResult executeRepairExisting(
        const PreservationRepairReason& reason,
        const DisplacedLivePreservationContext& context
    );

    bool atomicSwapAuthFiles(
        const QString& sourceHomePath,
        const QString& targetHomePath,
        QString& errorMessage
    );
};
