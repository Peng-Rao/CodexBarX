#pragma once

#include "CodexIdentity.h"
#include "CodexDisplacedLivePreservation.h"
#include "ManagedCodexAccount.h"
#include "CodexSystemAccountObserver.h"

#include <QString>
#include <QHash>
#include <optional>

struct CodexAccountPromotionContext {
    // Target account to promote
    QString targetAccountId;
    ManagedCodexAccount targetAccount;
    QString targetHomePath;
    CodexIdentity targetIdentity;
    QString targetEmail;
    bool targetCredentialsValid = false;

    // Live system account (may be displaced)
    std::optional<ObservedSystemCodexAccount> liveAccount;
    QString liveHomePath;

    // Existing managed accounts (for identity matching)
    QHash<QString, CodexIdentity> existingAccountIdentities;
    QHash<QString, QString> existingAccountHomePaths;

    // Environment for file operations
    QHash<QString, QString> env;

    // Validation result
    bool valid = false;
    QString errorMessage;

    static CodexAccountPromotionContext build(
        const QString& accountId,
        const QHash<QString, QString>& env,
        const ManagedCodexAccountStore& store,
        const CodexSystemAccountObserver& observer
    );
};

class CodexAccountPromotionContextBuilder {
public:
    CodexAccountPromotionContext build(
        const QString& accountId,
        const QHash<QString, QString>& env
    ) const;

private:
    CodexIdentity resolveIdentity(
        const ManagedCodexAccount& account,
        const QHash<QString, QString>& scopedEnv
    ) const;
};
