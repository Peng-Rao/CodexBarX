#include "CodexHistoryOwnership.h"

#include <QCryptographicHash>

const QString CodexHistoryOwnership::PREFIX_CANONICAL = QStringLiteral("codex:v1:provider-account:");
const QString CodexHistoryOwnership::PREFIX_EMAIL_HASH = QStringLiteral("codex:v1:email-hash:");

QString CodexHistoryOwnership::canonicalKey(const QString& providerAccountId)
{
    QString normalized = providerAccountId.trimmed();
    if (normalized.isEmpty()) {
        return QString();
    }
    return PREFIX_CANONICAL + normalized.toLower();
}

QString CodexHistoryOwnership::canonicalKeyFromEmail(const QString& email)
{
    QString normalized = email.trimmed().toLower();
    if (normalized.isEmpty()) {
        return QString();
    }
    return PREFIX_EMAIL_HASH + sha256(normalized);
}

std::pair<CodexHistoryPersistedOwner, QString> CodexHistoryOwnership::classifyPersistedKey(
    const QString& persistedKey,
    const QString& legacyEmailHashKey)
{
    if (persistedKey.isEmpty()) {
        return {CodexHistoryPersistedOwner::LegacyUnscoped, QString()};
    }

    // Check for canonical provider-account key
    if (persistedKey.startsWith(PREFIX_CANONICAL)) {
        QString accountId = persistedKey.mid(PREFIX_CANONICAL.length());
        return {CodexHistoryPersistedOwner::Canonical, accountId};
    }

    // Check for canonical email-hash key
    if (persistedKey.startsWith(PREFIX_EMAIL_HASH)) {
        QString hash = persistedKey.mid(PREFIX_EMAIL_HASH.length());
        return {CodexHistoryPersistedOwner::LegacyEmailHash, hash};
    }

    // Check for legacy email hash (direct SHA256 hash)
    if (persistedKey.length() == 64 && !legacyEmailHashKey.isEmpty()) {
        // Could be a legacy SHA256 hash of email
        QString expectedHash = sha256(legacyEmailHashKey.trimmed().toLower());
        if (persistedKey.toLower() == expectedHash.toLower()) {
            return {CodexHistoryPersistedOwner::LegacyEmailHash, persistedKey};
        }
    }

    // Check for scoped key format (any non-empty string that's not canonical)
    if (persistedKey.contains(QStringLiteral(":")) ||
        persistedKey.contains(QStringLiteral("/"))) {
        return {CodexHistoryPersistedOwner::LegacyOpaqueScoped, persistedKey};
    }

    // Default to unscoped
    return {CodexHistoryPersistedOwner::LegacyUnscoped, persistedKey};
}

bool CodexHistoryOwnership::belongsToTargetContinuity(
    const QString& persistedKey,
    const QString& targetCanonicalKey,
    const QString& canonicalEmailHashKey)
{
    if (persistedKey.isEmpty() || targetCanonicalKey.isEmpty()) {
        return false;
    }

    // Direct match with canonical key
    if (persistedKey == targetCanonicalKey) {
        return true;
    }

    // Match with email hash key if provided
    if (!canonicalEmailHashKey.isEmpty() && persistedKey == canonicalEmailHashKey) {
        return true;
    }

    // Classify and check legacy keys
    auto [ownerType, value] = classifyPersistedKey(persistedKey, canonicalEmailHashKey);

    switch (ownerType) {
    case CodexHistoryPersistedOwner::Canonical:
        // Exact canonical match already checked above
        return false;

    case CodexHistoryPersistedOwner::LegacyEmailHash:
        // Check if the hash matches the email hash key
        if (!canonicalEmailHashKey.isEmpty()) {
            QString expectedHash = canonicalEmailHashKey.mid(PREFIX_EMAIL_HASH.length());
            return value.toLower() == expectedHash.toLower();
        }
        return false;

    case CodexHistoryPersistedOwner::LegacyOpaqueScoped:
    case CodexHistoryPersistedOwner::LegacyUnscoped:
        // Legacy keys don't have clear ownership
        return false;
    }

    return false;
}

bool CodexHistoryOwnership::hasStrictSingleAccountContinuity(
    const QStringList& persistedKeys,
    const QString& targetCanonicalKey,
    const QString& canonicalEmailHashKey)
{
    if (persistedKeys.isEmpty()) {
        return true; // No history = no conflict
    }

    if (targetCanonicalKey.isEmpty()) {
        return false;
    }

    for (const QString& key : persistedKeys) {
        if (!belongsToTargetContinuity(key, targetCanonicalKey, canonicalEmailHashKey)) {
            return false;
        }
    }

    return true;
}

QString CodexHistoryOwnership::sha256(const QString& input)
{
    if (input.isEmpty()) {
        return QString();
    }

    QByteArray hash = QCryptographicHash::hash(
        input.toUtf8(),
        QCryptographicHash::Sha256
    );

    return hash.toHex();
}
