#include "ClaudeCLISession.h"
#include "../shared/ConPTYSession.h"
#include "../../util/BinaryLocator.h"

#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QProcessEnvironment>
#include <QFileInfo>

ClaudeCLISession::ClaudeCLISession(QObject* parent)
    : QObject(parent)
{
}

ClaudeCLISession::~ClaudeCLISession() = default;

bool ClaudeCLISession::isClaudeInstalled()
{
    return !resolveBinaryPath().isEmpty();
}

QString ClaudeCLISession::resolveBinaryPath()
{
    // Check environment variable override
    QString envPath = qEnvironmentVariable("CODEXBAR_CLAUDE_PATH");
    if (!envPath.isEmpty() && QFileInfo::exists(envPath)) {
        return envPath;
    }

    // Use BinaryLocator to find claude in PATH
    return BinaryLocator::resolve("claude");
}

ClaudeCLISession::CaptureResult ClaudeCLISession::captureUsage(int timeoutMs)
{
    return captureInternal("/usage", timeoutMs);
}

ClaudeCLISession::CaptureResult ClaudeCLISession::captureStatus(int timeoutMs)
{
    return captureInternal("/status", timeoutMs);
}

ClaudeCLISession::UsageStatusCaptureResult ClaudeCLISession::captureUsageAndStatus(int timeoutMs)
{
    UsageStatusCaptureResult result;

    const int totalTimeout = timeoutMs > 0 ? timeoutMs : m_timeoutMs;
    const int usageTimeout = qMax(3000, (totalTimeout * 2) / 3);
    const int statusTimeout = qMax(2000, totalTimeout - usageTimeout);

    auto usage = captureInternal(QStringLiteral("/usage"), usageTimeout);
    if (!usage.success) {
        result.errorMessage = usage.errorMessage;
        return result;
    }

    result.usageOutput = usage.output;

    auto status = captureInternal(QStringLiteral("/status"), statusTimeout);
    if (status.success) {
        result.statusOutput = status.output;
    } else {
        qDebug() << "[ClaudeCLISession] Status capture failed; continuing with usage output:"
                 << status.errorMessage;
    }

    result.success = true;
    return result;
}

void ClaudeCLISession::setEnvironment(const QHash<QString, QString>& env)
{
    m_env = env;
}

void ClaudeCLISession::setTimeout(int timeoutMs)
{
    m_timeoutMs = timeoutMs;
}

ClaudeCLISession::CaptureResult ClaudeCLISession::captureInternal(const QString& subcommand, int timeoutMs)
{
    CaptureResult result;

    QString binary = resolveBinaryPath();
    if (binary.isEmpty()) {
        result.errorMessage = "Claude CLI not found in PATH. Install from https://claude.ai/download";
        return result;
    }

    if (!ConPTYSession::isConPtyAvailable()) {
        result.errorMessage = "ConPTY is not available on this Windows version (requires Windows 10 1809+).";
        return result;
    }

    ConPTYSession session;
    QStringList args;
    args << "--no-alt-screen";

    QProcessEnvironment processEnv;
    for (auto it = m_env.constBegin(); it != m_env.constEnd(); ++it) {
        processEnv.insert(it.key(), it.value());
    }

    qDebug() << "[ClaudeCLISession] Starting ConPTY session:" << binary << args.join(' ');
    if (!session.start(binary, args, processEnv, m_cols, m_rows)) {
        result.errorMessage = "Failed to start Claude CLI session";
        return result;
    }

    // Wait for CLI to initialize
    QThread::msleep(500);

    if (!session.isRunning()) {
        result.errorMessage = "Claude CLI exited before we could send command";
        return result;
    }

    result = captureCommand(session, subcommand, timeoutMs);

    if (session.isRunning()) {
        session.terminate();
    }

    if (result.success) {
        QByteArray remaining = session.readOutput(500);
        if (!remaining.isEmpty()) {
            result.output.append(QString::fromUtf8(remaining));
        }
    }

    return result;
}

ClaudeCLISession::CaptureResult ClaudeCLISession::captureCommand(
    ConPTYSession& session,
    const QString& subcommand,
    int timeoutMs)
{
    CaptureResult result;

    QString cmd = subcommand + "\r\n";
    session.write(cmd.toUtf8());
    qDebug() << "[ClaudeCLISession] Sent command:" << subcommand;

    QByteArray accumulatedOutput;
    QDateTime deadline = QDateTime::currentDateTimeUtc().addMSecs(timeoutMs > 0 ? timeoutMs : m_timeoutMs);
    QDateTime lastOutputAt;
    bool hasRelevantOutput = false;
    bool trustAccepted = false;

    while (QDateTime::currentDateTimeUtc() < deadline) {
        QByteArray chunk = session.readOutput(250);
        if (!chunk.isEmpty()) {
            accumulatedOutput.append(chunk);
            lastOutputAt = QDateTime::currentDateTimeUtc();

            QString output = QString::fromUtf8(accumulatedOutput);
            const QString lower = output.toLower();
            if (!trustAccepted &&
                lower.contains(QStringLiteral("do you trust")) &&
                !lower.contains(QStringLiteral("current session"))) {
                qDebug() << "[ClaudeCLISession] Detected trust prompt, sending 'y'";
                session.write("y\r\n");
                trustAccepted = true;
            }

            hasRelevantOutput = hasRelevantOutput ||
                                hasRelevantCommandOutput(subcommand, output);
        }

        if (hasRelevantOutput && lastOutputAt.isValid() &&
            lastOutputAt.msecsTo(QDateTime::currentDateTimeUtc()) >= 900) {
            break;
        }

        if (!session.isRunning()) {
            qDebug() << "[ClaudeCLISession] Session ended";
            break;
        }
    }

    qDebug() << "[ClaudeCLISession] Captured" << subcommand
             << "output length:" << accumulatedOutput.length();

    if (accumulatedOutput.isEmpty()) {
        result.errorMessage = QStringLiteral("No output from Claude CLI for %1").arg(subcommand);
        return result;
    }

    result.output = QString::fromUtf8(accumulatedOutput);
    result.success = true;
    return result;
}

bool ClaudeCLISession::hasRelevantCommandOutput(const QString& subcommand, const QString& output) const
{
    const QString lower = output.toLower();
    const QString compact = lower;

    if (lower.contains(QStringLiteral("token_expired")) ||
        lower.contains(QStringLiteral("authentication_error")) ||
        lower.contains(QStringLiteral("not logged in")) ||
        lower.contains(QStringLiteral("failed to load usage data"))) {
        return true;
    }

    if (subcommand == QLatin1String("/usage")) {
        return lower.contains(QStringLiteral("current session")) &&
               lower.contains(QLatin1Char('%'));
    }

    if (subcommand == QLatin1String("/status")) {
        return lower.contains(QStringLiteral("account:")) ||
               lower.contains(QStringLiteral("email:")) ||
               lower.contains(QStringLiteral("organization:")) ||
               lower.contains(QStringLiteral("login method:")) ||
               lower.contains(QStringLiteral("claude max")) ||
               lower.contains(QStringLiteral("claude pro")) ||
               lower.contains(QStringLiteral("claude team")) ||
               lower.contains(QStringLiteral("claude enterprise")) ||
               lower.contains(QStringLiteral("claude ultra")) ||
               compact.contains(QStringLiteral("settings: status"));
    }

    return !output.trimmed().isEmpty();
}

bool ClaudeCLISession::handleTrustPrompt(ConPTYSession& session, const QString& output)
{
    Q_UNUSED(session)
    Q_UNUSED(output)
    // Trust prompt handling is done inline in captureInternal
    return false;
}
