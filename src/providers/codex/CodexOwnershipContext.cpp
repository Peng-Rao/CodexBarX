#include "CodexOwnershipContext.h"

// CodexOwnershipContextBuilder

CodexOwnershipContext CodexOwnershipContextBuilder::build(
    const QString& preferredEmail,
    const QString& providerAccountId,
    const QDateTime& weeklyResetAt,
    bool hasMultiAccountVeto)
{
    CodexOwnershipContext context;

    if (!providerAccountId.isEmpty()) {
        context.canonicalKey = CodexHistoryOwnership::canonicalKey(providerAccountId);
    }

    if (!preferredEmail.isEmpty()) {
        context.canonicalEmailHashKey = CodexHistoryOwnership::canonicalKeyFromEmail(preferredEmail);
    }

    context.currentWeeklyResetAt = weeklyResetAt;
    context.hasAdjacentMultiAccountVeto = hasMultiAccountVeto;

    return context;
}

CodexOwnershipContext CodexOwnershipContextBuilder::buildWithDashboardFallback(
    const QString& preferredEmail,
    const QString& providerAccountId,
    const QDateTime& weeklyResetAt,
    bool hasMultiAccountVeto,
    const QString& dashboardEmail)
{
    CodexOwnershipContext context = build(
        preferredEmail, providerAccountId, weeklyResetAt, hasMultiAccountVeto);

    // Use dashboard email as fallback if preferred email is empty
    QString fallbackEmail = preferredEmail.isEmpty() ? dashboardEmail : preferredEmail;
    if (!fallbackEmail.isEmpty()) {
        context.planUtilizationLegacyEmailHash = CodexHistoryOwnership::canonicalKeyFromEmail(fallbackEmail);
    }

    return context;
}

// CodexHistoryOwnershipExtended

bool CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
    const QStringList& persistedKeys,
    const QString& targetCanonicalKey,
    const QString& canonicalEmailHashKey,
    const QString& legacyEmailHash,
    bool hasAdjacentMultiAccountVeto)
{
    // Multi-account veto: if adjacent accounts exist, continuity is broken
    if (hasAdjacentMultiAccountVeto) {
        return false;
    }

    if (persistedKeys.isEmpty()) {
        return true; // No history = no conflict
    }

    if (targetCanonicalKey.isEmpty() && canonicalEmailHashKey.isEmpty()) {
        return false;
    }

    for (const QString& key : persistedKeys) {
        // Check canonical match
        if (!targetCanonicalKey.isEmpty() && key == targetCanonicalKey) {
            continue;
        }

        // Check email hash match
        if (!canonicalEmailHashKey.isEmpty() && key == canonicalEmailHashKey) {
            continue;
        }

        // Check legacy email hash match
        if (!legacyEmailHash.isEmpty()) {
            auto [ownerType, value] = classifyPersistedKey(key, legacyEmailHash);
            if (ownerType == CodexHistoryPersistedOwner::LegacyEmailHash) {
                QString expectedHash = sha256(legacyEmailHash.trimmed().toLower());
                if (value.toLower() == expectedHash.toLower()) {
                    continue;
                }
            }
        }

        // Check using base class method as fallback
        if (CodexHistoryOwnership::belongsToTargetContinuity(key, targetCanonicalKey, canonicalEmailHashKey)) {
            continue;
        }

        // Key doesn't belong to target
        return false;
    }

    return true;
}

bool CodexHistoryOwnershipExtended::belongsToTargetContinuity(
    const QString& persistedKey,
    const CodexOwnershipContext& context)
{
    if (persistedKey.isEmpty()) {
        return false;
    }

    // Direct canonical match
    if (!context.canonicalKey.isEmpty() && persistedKey == context.canonicalKey) {
        return true;
    }

    // Email hash match
    if (!context.canonicalEmailHashKey.isEmpty() && persistedKey == context.canonicalEmailHashKey) {
        return true;
    }

    // Legacy email hash match
    if (!context.historicalLegacyEmailHash.isEmpty()) {
        auto [ownerType, value] = classifyPersistedKey(persistedKey, context.historicalLegacyEmailHash);
        if (ownerType == CodexHistoryPersistedOwner::LegacyEmailHash) {
            QString expectedHash = sha256(context.historicalLegacyEmailHash.trimmed().toLower());
            return value.toLower() == expectedHash.toLower();
        }
    }

    // Dashboard fallback
    if (!context.planUtilizationLegacyEmailHash.isEmpty() &&
        persistedKey == context.planUtilizationLegacyEmailHash) {
        return true;
    }

    return false;
}

bool CodexHistoryOwnershipExtended::hasStrictSingleAccountContinuity(
    const QStringList& persistedKeys,
    const CodexOwnershipContext& context)
{
    if (context.hasAdjacentMultiAccountVeto) {
        return false;
    }

    if (persistedKeys.isEmpty()) {
        return true;
    }

    if (!context.isValid()) {
        return false;
    }

    for (const QString& key : persistedKeys) {
        if (!belongsToTargetContinuity(key, context)) {
            return false;
        }
    }

    return true;
}
