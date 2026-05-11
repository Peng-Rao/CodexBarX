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

    // Send the subcommand
    QString cmd = subcommand + "\r\n";
    session.write(cmd.toUtf8());
    qDebug() << "[ClaudeCLISession] Sent command:" << subcommand;

    // Wait for output
    QThread::msleep(300);

    QByteArray accumulatedOutput;
    QDateTime deadline = QDateTime::currentDateTimeUtc().addMSecs(timeoutMs);

    // Stop substrings for /usage
    QStringList stopSubstrings;
    if (subcommand == "/usage") {
        stopSubstrings = {
            "Current week (all models)",
            "Current week (Opus)",
            "Current week (Sonnet)",
            "Current session",
            "Failed to load usage data",
            "failed to load usage data"
        };
    }

    while (QDateTime::currentDateTimeUtc() < deadline) {
        QByteArray chunk = session.readOutput(500);
        if (!chunk.isEmpty()) {
            accumulatedOutput.append(chunk);

            // Check for stop substrings
            QString current = QString::fromUtf8(accumulatedOutput);
            for (const QString& stop : stopSubstrings) {
                if (current.contains(stop, Qt::CaseInsensitive)) {
                    // Wait a bit more for the panel to fully render
                    QThread::msleep(800);
                    QByteArray more = session.readOutput(500);
                    accumulatedOutput.append(more);
                    break;
                }
            }
        }

        if (!session.isRunning()) {
            qDebug() << "[ClaudeCLISession] Session ended";
            break;
        }

        QThread::msleep(50);
    }

    // Check for trust prompt
    QString outputStr = QString::fromUtf8(accumulatedOutput);
    if (outputStr.toLower().contains("do you trust") && !outputStr.toLower().contains("current session")) {
        qDebug() << "[ClaudeCLISession] Detected trust prompt, sending 'y'";
        session.write("y\r\n");
        QThread::msleep(1000);

        // Continue capturing after accepting trust prompt
        while (QDateTime::currentDateTimeUtc() < deadline) {
            QByteArray chunk = session.readOutput(500);
            if (!chunk.isEmpty()) {
                accumulatedOutput.append(chunk);
            }
            if (!session.isRunning()) break;
            QThread::msleep(50);
        }
    }

    if (session.isRunning()) {
        session.terminate();
    }

    QByteArray remaining = session.readOutput(1000);
    accumulatedOutput.append(remaining);

    qDebug() << "[ClaudeCLISession] Total output length:" << accumulatedOutput.length();

    if (accumulatedOutput.isEmpty()) {
        result.errorMessage = "No output from Claude CLI";
        return result;
    }

    result.output = QString::fromUtf8(accumulatedOutput);
    result.success = true;
    return result;
}

bool ClaudeCLISession::handleTrustPrompt(ConPTYSession& session, const QString& output)
{
    Q_UNUSED(session)
    Q_UNUSED(output)
    // Trust prompt handling is done inline in captureInternal
    return false;
}
