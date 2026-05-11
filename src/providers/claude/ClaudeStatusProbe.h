#pragma once

#include <QString>
#include <QDateTime>
#include <optional>

struct ClaudeStatusSnapshot {
    // Session (5-hour window)
    std::optional<int> sessionPercentLeft;
    QString sessionResetDescription;

    // Weekly (all models)
    std::optional<int> weeklyPercentLeft;
    QString weeklyResetDescription;

    // Opus weekly
    std::optional<int> opusPercentLeft;
    QString opusResetDescription;

    // Account info
    std::optional<QString> accountEmail;
    std::optional<QString> accountOrganization;
    std::optional<QString> loginMethod;

    // Raw output for debugging
    QString rawText;

    bool isValid() const {
        return sessionPercentLeft.has_value() ||
               weeklyPercentLeft.has_value() ||
               opusPercentLeft.has_value();
    }
};

class ClaudeStatusProbe {
public:
    enum class ParseError {
        None,
        EmptyOutput,
        NotLoggedIn,
        TokenExpired,
        ParseFailed,
        TrustPrompt
    };

    struct ParseResult {
        bool success = false;
        ClaudeStatusSnapshot snapshot;
        ParseError error = ParseError::None;
        QString errorMessage;
    };

    // Main parsing entry point
    static ParseResult parse(const QString& usageOutput);

    // Utility methods
    static bool hasRelevantOutput(const QString& text);
    static std::optional<QString> extractError(const QString& text);

private:
    // Normalization for label search
    static QString normalizedForLabelSearch(const QString& text);

    // Section extraction
    static QString extractSection(const QString& label, const QString& text);

    // Value parsers
    static std::optional<int> extractPercent(const QString& label, const QString& text);
    static std::optional<int> percentFromLine(const QString& line);
    static QString extractResetDescription(const QString& label, const QString& text);
    static std::optional<QString> resetFromLine(const QString& line);

    // Account parsers
    static std::optional<QString> extractEmail(const QString& text);
    static std::optional<QString> extractOrganization(const QString& text);
    static std::optional<QString> extractLoginMethod(const QString& text);

    // Error detection
    static bool isNotLoggedInError(const QString& text);
    static bool isTokenExpiredError(const QString& text);
    static bool isTrustPrompt(const QString& text);

    // Trim to latest usage panel
    static QString trimToLatestUsagePanel(const QString& text);

    // All percentages fallback
    static QVector<int> allPercents(const QString& text);

    // Clean reset line
    static QString cleanResetLine(const QString& raw);
};
