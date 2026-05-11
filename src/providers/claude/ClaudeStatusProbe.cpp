#include "ClaudeStatusProbe.h"
#include "../../util/TextParser.h"

#include <QRegularExpression>
#include <QStringList>

ClaudeStatusProbe::ParseResult ClaudeStatusProbe::parse(const QString& usageOutput, const QString& statusOutput)
{
    ParseResult result;

    // Strip ANSI escape codes
    QString clean = TextParser::stripAnsiEscapes(usageOutput);
    QString cleanStatus = TextParser::stripAnsiEscapes(statusOutput);
    QString combined = clean;
    if (!cleanStatus.isEmpty()) {
        if (!combined.isEmpty()) combined.append('\n');
        combined.append(cleanStatus);
    }

    if (combined.isEmpty()) {
        result.error = ParseError::EmptyOutput;
        result.errorMessage = "Empty output from Claude CLI";
        return result;
    }

    // Check for errors first
    if (auto error = extractError(combined)) {
        result.errorMessage = *error;
        if (isNotLoggedInError(combined)) {
            result.error = ParseError::NotLoggedIn;
        } else if (isTokenExpiredError(combined)) {
            result.error = ParseError::TokenExpired;
        } else if (isTrustPrompt(combined)) {
            result.error = ParseError::TrustPrompt;
        } else {
            result.error = ParseError::ParseFailed;
        }
        return result;
    }

    // Trim to latest usage panel if available
    QString panelText = trimToLatestUsagePanel(clean);
    if (panelText.isEmpty()) {
        panelText = clean;
    }

    // Check if output looks relevant
    if (!hasRelevantOutput(panelText)) {
        result.error = ParseError::ParseFailed;
        result.errorMessage = "No usage data found in CLI output";
        return result;
    }

    // Extract percentages
    result.snapshot.sessionPercentLeft = extractPercent("Current session", panelText);
    result.snapshot.weeklyPercentLeft = extractPercent("Current week (all models)", panelText);
    result.snapshot.opusPercentLeft = extractPercent("Current week (Opus)", panelText);

    // Fallback: try Sonnet label for Opus
    if (!result.snapshot.opusPercentLeft.has_value()) {
        result.snapshot.opusPercentLeft = extractPercent("Current week (Sonnet)", panelText);
    }

    // Fallback: order-based percent scraping when labels exist but layout changed
    QString compact = panelText.toLower().remove(QRegularExpression("\\s"));
    bool hasWeeklyLabel = compact.contains("currentweek");
    bool hasOpusLabel = compact.contains("opus") || compact.contains("sonnet");

    if (!result.snapshot.sessionPercentLeft.has_value() ||
        (hasWeeklyLabel && !result.snapshot.weeklyPercentLeft.has_value()) ||
        (hasOpusLabel && !result.snapshot.opusPercentLeft.has_value())) {

        QVector<int> ordered = allPercents(panelText);
        if (!result.snapshot.sessionPercentLeft.has_value() && ordered.size() > 0) {
            result.snapshot.sessionPercentLeft = ordered[0];
        }
        if (hasWeeklyLabel && !result.snapshot.weeklyPercentLeft.has_value() && ordered.size() > 1) {
            result.snapshot.weeklyPercentLeft = ordered[1];
        }
        if (hasOpusLabel && !result.snapshot.opusPercentLeft.has_value() && ordered.size() > 2) {
            result.snapshot.opusPercentLeft = ordered[2];
        }
    }

    // Must have at least session percentage
    if (!result.snapshot.sessionPercentLeft.has_value()) {
        result.error = ParseError::ParseFailed;
        result.errorMessage = "Could not find Current session percentage";
        return result;
    }

    // Extract reset descriptions
    result.snapshot.sessionResetDescription = extractResetDescription("Current session", panelText);
    result.snapshot.weeklyResetDescription = extractResetDescription("Current week (all models)", panelText);
    result.snapshot.opusResetDescription = extractResetDescription("Current week (Opus)", panelText);

    // Extract account info
    const QString identityText = cleanStatus.isEmpty() ? clean : cleanStatus + '\n' + clean;
    result.snapshot.accountEmail = extractEmail(identityText);
    result.snapshot.accountOrganization = extractOrganization(identityText);
    result.snapshot.loginMethod = extractLoginMethod(identityText);

    result.snapshot.rawText = combined;
    result.success = true;
    return result;
}

bool ClaudeStatusProbe::hasRelevantOutput(const QString& text)
{
    QString compact = text.toLower().remove(QRegularExpression("\\s"));
    return compact.contains("currentsession") ||
           compact.contains("currentweek") ||
           compact.contains("loadingusage") ||
           compact.contains("failedtoloadusagedata");
}

std::optional<QString> ClaudeStatusProbe::extractError(const QString& text)
{
    QString lower = text.toLower();
    QString compact = lower.remove(QRegularExpression("\\s"));

    if (isTrustPrompt(text)) {
        return "Claude CLI is waiting for a folder trust prompt. Open claude once and choose 'Yes, proceed'.";
    }

    if (isNotLoggedInError(text)) {
        return "Claude CLI is not logged in. Run `claude login`.";
    }

    if (lower.contains("token_expired") || lower.contains("token has expired")) {
        return "Claude CLI token expired. Run `claude login` to refresh.";
    }

    if (lower.contains("authentication_error")) {
        return "Claude CLI authentication error. Run `claude login`.";
    }

    if (lower.contains("rate_limit_error") || lower.contains("rate limited") || compact.contains("ratelimited")) {
        return "Claude CLI usage endpoint is rate limited. Please try again later.";
    }

    if (lower.contains("failed to load usage data") || compact.contains("failedtoloadusagedata")) {
        return "Claude CLI could not load usage data. Open the CLI and retry `/usage`.";
    }

    return std::nullopt;
}

QString ClaudeStatusProbe::normalizedForLabelSearch(const QString& text)
{
    QString result;
    for (const QChar& c : text.toLower()) {
        if (c.isLetterOrNumber()) {
            result.append(c);
        }
    }
    return result;
}

QString ClaudeStatusProbe::extractSection(const QString& label, const QString& text)
{
    QStringList lines = text.split('\n');
    QString normalizedLabel = normalizedForLabelSearch(label);

    for (int i = 0; i < lines.size(); ++i) {
        QString normalizedLine = normalizedForLabelSearch(lines[i]);
        if (normalizedLine.contains(normalizedLabel)) {
            // Return next 12 lines as section
            QString section;
            for (int j = i; j < qMin(i + 12, lines.size()); ++j) {
                section += lines[j] + "\n";
            }
            return section;
        }
    }
    return QString();
}

std::optional<int> ClaudeStatusProbe::extractPercent(const QString& label, const QString& text)
{
    QString section = extractSection(label, text);
    if (section.isEmpty()) return std::nullopt;

    QStringList lines = section.split('\n');
    for (const QString& line : lines) {
        if (auto pct = percentFromLine(line)) {
            return pct;
        }
    }
    return std::nullopt;
}

std::optional<int> ClaudeStatusProbe::percentFromLine(const QString& line)
{
    // Check if this is likely a status context line (contains | and model tokens)
    if (line.contains("|")) {
        QString lower = line.toLower();
        QStringList modelTokens = {"opus", "sonnet", "haiku", "default"};
        for (const QString& token : modelTokens) {
            if (lower.contains(token)) {
                return std::nullopt;  // Skip status context line
            }
        }
    }

    // Match percentage pattern: digits with optional decimal, followed by optional whitespace and %
    static QRegularExpression re("(\\d{1,3}(?:\\.\\d+)?)\\s*%");
    QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch()) return std::nullopt;

    bool ok;
    double rawVal = match.captured(1).toDouble(&ok);
    if (!ok) return std::nullopt;

    int clamped = qBound(0, static_cast<int>(rawVal), 100);

    QString lower = line.toLower();
    bool isUsed = lower.contains("used") || lower.contains("spent") || lower.contains("consumed");
    bool isRemaining = lower.contains("left") || lower.contains("remaining") || lower.contains("available");

    if (isUsed) {
        return 100 - clamped;  // Convert "used" to "left"
    }
    if (isRemaining) {
        return clamped;
    }
    return std::nullopt;  // Ambiguous, don't guess
}

QString ClaudeStatusProbe::extractResetDescription(const QString& label, const QString& text)
{
    QString section = extractSection(label, text);
    if (section.isEmpty()) return QString();

    QStringList lines = section.split('\n');
    for (const QString& line : lines) {
        if (auto reset = resetFromLine(line)) {
            return *reset;
        }
    }
    return QString();
}

std::optional<QString> ClaudeStatusProbe::resetFromLine(const QString& line)
{
    static QRegularExpression re("Resets\\s+(.+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = re.match(line);
    if (!match.hasMatch()) return std::nullopt;

    return cleanResetLine(match.captured(1));
}

QString ClaudeStatusProbe::cleanResetLine(const QString& raw)
{
    QString cleaned = raw.trimmed();
    // Remove trailing parenthesis and space
    while (cleaned.endsWith(')') || cleaned.endsWith(' ')) {
        cleaned.chop(1);
    }
    // Balance parentheses
    int openCount = cleaned.count('(');
    int closeCount = cleaned.count(')');
    if (openCount > closeCount) {
        cleaned.append(')');
    }
    return cleaned;
}

std::optional<QString> ClaudeStatusProbe::extractEmail(const QString& text)
{
    // Try Account: pattern first
    static QRegularExpression accountRe("Account:\\s*([^\\s@]+@[^\\s@]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = accountRe.match(text);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    // Try Email: pattern
    static QRegularExpression emailRe("Email:\\s*([^\\s@]+@[^\\s@]+)", QRegularExpression::CaseInsensitiveOption);
    match = emailRe.match(text);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    // Fallback: general email pattern
    static QRegularExpression generalRe("[A-Z0-9._%+-]+@[A-Z0-9.-]+\\.[A-Z]{2,}", QRegularExpression::CaseInsensitiveOption);
    match = generalRe.match(text);
    if (match.hasMatch()) {
        return match.captured(0).trimmed();
    }

    return std::nullopt;
}

std::optional<QString> ClaudeStatusProbe::extractOrganization(const QString& text)
{
    static QRegularExpression orgRe("Org:\\s*(.+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = orgRe.match(text);
    if (match.hasMatch()) {
        QString org = match.captured(1).trimmed();
        // Suppress org if it's just the email prefix
        auto email = extractEmail(text);
        if (email.has_value() && org.toLower().startsWith(email->toLower().split('@').value(0))) {
            return std::nullopt;
        }
        return org;
    }

    static QRegularExpression org2Re("Organization:\\s*(.+)", QRegularExpression::CaseInsensitiveOption);
    match = org2Re.match(text);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    return std::nullopt;
}

std::optional<QString> ClaudeStatusProbe::extractLoginMethod(const QString& text)
{
    // Try explicit login method
    static QRegularExpression loginRe("Login\\s+Method:\\s*(.+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = loginRe.match(text);
    if (match.hasMatch()) {
        return match.captured(1).trimmed();
    }

    // Try Claude <Plan> pattern
    static QRegularExpression planRe("(Claude\\s+[a-zA-Z0-9][a-zA-Z0-9\\s._-]{0,24})", QRegularExpression::CaseInsensitiveOption);
    match = planRe.match(text);
    if (match.hasMatch()) {
        QString plan = match.captured(1).trimmed();
        // Filter out version strings
        QString lower = plan.toLower();
        if (!lower.contains("code v") && !lower.contains("code version") && !lower.contains("code")) {
            return plan;
        }
    }

    return std::nullopt;
}

bool ClaudeStatusProbe::isNotLoggedInError(const QString& text)
{
    QString lower = text.toLower();
    return lower.contains("not logged in") ||
           lower.contains("authentication required");
}

bool ClaudeStatusProbe::isTokenExpiredError(const QString& text)
{
    QString lower = text.toLower();
    return lower.contains("token_expired") ||
           lower.contains("token has expired") ||
           lower.contains("authentication_error");
}

bool ClaudeStatusProbe::isTrustPrompt(const QString& text)
{
    QString lower = text.toLower();
    return lower.contains("do you trust") && !lower.contains("current session");
}

QString ClaudeStatusProbe::trimToLatestUsagePanel(const QString& text)
{
    // Find the last occurrence of "Settings:" that has "Usage" after it
    int lastSettings = text.lastIndexOf("Settings:", -1, Qt::CaseInsensitive);
    if (lastSettings < 0) return QString();

    QString tail = text.mid(lastSettings);
    if (!tail.contains("Usage", Qt::CaseInsensitive)) return QString();

    QString lower = tail.toLower();
    bool hasPercent = lower.contains("%");
    bool hasUsageWords = lower.contains("used") || lower.contains("left") ||
                         lower.contains("remaining") || lower.contains("available");
    bool hasLoading = lower.contains("loading usage");

    if ((hasPercent && hasUsageWords) || hasLoading) {
        return tail;
    }
    return QString();
}

QVector<int> ClaudeStatusProbe::allPercents(const QString& text)
{
    QVector<int> results;

    QString compact = text.toLower().remove(QRegularExpression("\\s"));
    bool hasUsageWindows = compact.contains("currentsession") || compact.contains("currentweek");
    bool hasLoading = compact.contains("loadingusage");
    bool hasUsagePercentKeywords = compact.contains("used") || compact.contains("left") ||
                                    compact.contains("remaining") || compact.contains("available");

    bool loadingOnly = hasLoading && !hasUsageWindows;
    if (!hasUsageWindows && !hasLoading) return results;
    if (loadingOnly) return results;
    if (!hasUsagePercentKeywords) return results;

    QStringList lines = text.split('\n');
    for (const QString& line : lines) {
        if (auto pct = percentFromLine(line)) {
            results.append(*pct);
        }
    }

    return results;
}
