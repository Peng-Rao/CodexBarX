#pragma once

#include "CodexHistoryOwnership.h"
#include <QDateTime>
#include <QString>
#include <QVariantMap>

/**
 * @brief Context structure for Codex account ownership decisions.
 *
 * Contains information about the current account ownership state,
 * including weekly reset tracking and multi-account veto detection.
 */
struct CodexOwnershipContext {
    QString canonicalKey;                    // Primary canonical key for the account
    QString canonicalEmailHashKey;           // Email hash canonical key
    QString historicalLegacyEmailHash;       // Legacy email hash from historical data
    QString planUtilizationLegacyEmailHash;  // Legacy email hash from plan utilization
    QDateTime currentWeeklyResetAt;          // Current weekly reset timestamp
    bool hasAdjacentMultiAccountVeto = false; // True if multiple accounts detected

    bool isValid() const {
        return !canonicalKey.isEmpty() || !canonicalEmailHashKey.isEmpty();
    }

    /**
     * @brief Check if this context has a dashboard fallback email.
     */
    bool hasDashboardFallback() const {
        return !planUtilizationLegacyEmailHash.isEmpty();
    }
};

/**
 * @brief Utility class for building and working with CodexOwnershipContext.
 */
class CodexOwnershipContextBuilder {
public:
    /**
     * @brief Build ownership context from email and optional provider account ID.
     */
    static CodexOwnershipContext build(
        const QString& preferredEmail,
        const QString& providerAccountId = QString(),
        const QDateTime& weeklyResetAt = QDateTime(),
        bool hasMultiAccountVeto = false
    );

    /**
     * @brief Build ownership context with dashboard fallback.
     */
    static CodexOwnershipContext buildWithDashboardFallback(
        const QString& preferredEmail,
        const QString& providerAccountId = QString(),
        const QDateTime& weeklyResetAt = QDateTime(),
        bool hasMultiAccountVeto = false,
        const QString& dashboardEmail = QString()
    );
};

/**
 * @brief Extended history ownership checks with context support.
 */
class CodexHistoryOwnershipExtended : public CodexHistoryOwnership {
public:
    /**
     * @brief Check if history strictly belongs to single account with full context.
     *
     * Enhanced version of hasStrictSingleAccountContinuity with additional parameters:
     * - legacyEmailHash: Direct legacy email hash for matching
     * - hasAdjacentMultiAccountVeto: Whether to veto due to multiple adjacent accounts
     */
    static bool hasStrictSingleAccountContinuity(
        const QStringList& persistedKeys,
        const QString& targetCanonicalKey,
        const QString& canonicalEmailHashKey = QString(),
        const QString& legacyEmailHash = QString(),
        bool hasAdjacentMultiAccountVeto = false
    );

    /**
     * @brief Check if history belongs to target using full ownership context.
     */
    static bool belongsToTargetContinuity(
        const QString& persistedKey,
        const CodexOwnershipContext& context
    );

    /**
     * @brief Check if history strictly belongs to single account using full context.
     */
    static bool hasStrictSingleAccountContinuity(
        const QStringList& persistedKeys,
        const CodexOwnershipContext& context
    );
};
