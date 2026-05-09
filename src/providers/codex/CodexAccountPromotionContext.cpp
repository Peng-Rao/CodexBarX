#include "CodexAccountPromotionContext.h"
#include "CodexHomeScope.h"
#include "../../models/CodexUsageResponse.h"

#include <QDir>

CodexAccountPromotionContext CodexAccountPromotionContext::build(
    const QString& accountId,
    const QHash<QString, QString>& env,
    const ManagedCodexAccountStore& store,
    const CodexSystemAccountObserver& observer)
{
    CodexAccountPromotionContext ctx;
    ctx.env = env;

    // Load target account
    auto account = store.account(accountId);
    if (!account.has_value()) {
        ctx.errorMessage = QStringLiteral("Account not found: ") + accountId;
        return ctx;
    }

    if (account->id == QStringLiteral("live-system")) {
        ctx.errorMessage = QStringLiteral("Cannot promote live-system account");
        return ctx;
    }

    ctx.targetAccountId = accountId;
    ctx.targetAccount = *account;
    ctx.targetHomePath = account->managedHomePath;

    // Build scoped environment for target account
    QHash<QString, QString> scopedEnv = ctx.targetHomePath.isEmpty()
        ? env
        : CodexHomeScope::scopedEnvironment(env, ctx.targetHomePath);

    // Load and validate credentials
    auto credentials = CodexOAuthCredentials::load(scopedEnv);
    if (!credentials.has_value()) {
        ctx.errorMessage = QStringLiteral("No valid credentials for account: ") + accountId;
        return ctx;
    }

    ctx.targetCredentialsValid = true;

    // Resolve target identity from credentials
    QString email = credentials->idToken.isEmpty()
        ? account->email
        : QString(); // TODO: parse JWT for email

    ctx.targetIdentity = CodexIdentityResolver::resolve(credentials->accountId, email);
    ctx.targetEmail = CodexIdentity::normalizeEmail(email.isEmpty() ? account->email : email);

    // Load live system account
    ctx.liveAccount = observer.loadSystemAccount(env);
    ctx.liveHomePath = CodexHomeScope::ambientHomeURL(env);

    // Load existing managed accounts for identity matching
    auto allAccounts = store.loadAccounts();
    for (const auto& existingAccount : allAccounts) {
        if (existingAccount.id == accountId) continue; // Skip target

        QHash<QString, QString> existingScopedEnv = existingAccount.managedHomePath.isEmpty()
            ? env
            : CodexHomeScope::scopedEnvironment(env, existingAccount.managedHomePath);

        auto existingCreds = CodexOAuthCredentials::load(existingScopedEnv);
        QString existingEmail;
        if (existingCreds.has_value() && !existingCreds->idToken.isEmpty()) {
            // TODO: parse JWT for email
            existingEmail = existingAccount.email;
        } else {
            existingEmail = existingAccount.email;
        }

        CodexIdentity existingIdentity = CodexIdentityResolver::resolve(
            existingCreds.has_value() ? existingCreds->accountId : QString(),
            existingEmail
        );

        ctx.existingAccountIdentities[existingAccount.id] = existingIdentity;
        ctx.existingAccountHomePaths[existingAccount.id] = existingAccount.managedHomePath;
    }

    ctx.valid = true;
    return ctx;
}

CodexAccountPromotionContext CodexAccountPromotionContextBuilder::build(
    const QString& accountId,
    const QHash<QString, QString>& env) const
{
    ManagedCodexAccountStore store;
    CodexSystemAccountObserver observer;
    return CodexAccountPromotionContext::build(accountId, env, store, observer);
}

CodexIdentity CodexAccountPromotionContextBuilder::resolveIdentity(
    const ManagedCodexAccount& account,
    const QHash<QString, QString>& scopedEnv) const
{
    auto credentials = CodexOAuthCredentials::load(scopedEnv);
    if (!credentials.has_value()) {
        return CodexIdentity::unresolved();
    }

    return CodexIdentityResolver::resolve(
        credentials->accountId,
        account.email
    );
}
