#pragma once

#include <QString>
#include <QStringList>
#include <utility>

enum class CodexHistoryPersistedOwner {
    Canonical,          // codex:v1:provider-account:xxx
    LegacyEmailHash,    // SHA256 hash
    LegacyOpaqueScoped, // Unknown scoped key
    LegacyUnscoped      // No owner key
};

class CodexHistoryOwnership {
public:
    // Generate canonical owner key from provider account ID
    static QString canonicalKey(const QString& providerAccountId);

    // Generate canonical owner key from email
    static QString canonicalKeyFromEmail(const QString& email);

    // Classify a persisted key
    static std::pair<CodexHistoryPersistedOwner, QString> classifyPersistedKey(
        const QString& persistedKey,
        const QString& legacyEmailHashKey = QString()
    );

    // Check if history belongs to target account continuity
    static bool belongsToTargetContinuity(
        const QString& persistedKey,
        const QString& targetCanonicalKey,
        const QString& canonicalEmailHashKey = QString()
    );

    // Check if history strictly belongs to single account
    static bool hasStrictSingleAccountContinuity(
        const QStringList& persistedKeys,
        const QString& targetCanonicalKey,
        const QString& canonicalEmailHashKey = QString()
    );

private:
    static QString sha256(const QString& input);
    static const QString PREFIX_CANONICAL;
    static const QString PREFIX_EMAIL_HASH;
};
