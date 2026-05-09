#pragma once

#include <QString>
#include <QVariant>

/**
 * @brief Enum representing the source of a Codex account selection.
 *
 * Similar to Swift's CodexActiveSource enum:
 * - liveSystem: The system-wide Codex account
 * - managedAccount: A stored managed account
 */
enum class CodexActiveSource {
    LiveSystem,
    ManagedAccount
};

/**
 * @brief Utility for working with CodexActiveSource.
 */
class CodexActiveSourceUtil {
public:
    static QString toString(CodexActiveSource source);
    static CodexActiveSource fromString(const QString& str);
    static QVariant toVariant(CodexActiveSource source);
};
