#include "ClaudePersistentCLISession.h"
#include "../shared/ConPTYSession.h"

#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QProcessEnvironment>

ClaudePersistentCLISession& ClaudePersistentCLISession::instance()
{
    static ClaudePersistentCLISession session;
    return session;
}

ClaudePersistentCLISession::ClaudePersistentCLISession() {}

ClaudePersistentCLISession::~ClaudePersistentCLISession()
{
    cleanup();
}

bool ClaudePersistentCLISession::isActive() const
{
    return m_started && m_session && m_session->isRunning();
}

bool ClaudePersistentCLISession::ensureStarted(
    const QString& binary,
    int cols,
    int rows,
    const QHash<QString, QString>& env)
{
    // Reuse existing session if parameters match
    if (m_started && m_session && m_session->isRunning() &&
        m_binaryPath == binary && m_cols == cols && m_rows == rows && m_env == env) {
        return true;
    }

    cleanup();

    m_session = new ConPTYSession();
    m_binaryPath = binary;
    m_cols = cols;
    m_rows = rows;
    m_env = env;
    m_trustAccepted = false;

    QStringList args;
    args << "--no-alt-screen";

    QProcessEnvironment processEnv;
    for (auto it = env.constBegin(); it != env.constEnd(); ++it) {
        processEnv.insert(it.key(), it.value());
    }

    qDebug() << "[ClaudePersistentCLI] Starting session:" << binary << args.join(' ');
    if (!m_session->start(binary, args, processEnv, cols, rows)) {
        qDebug() << "[ClaudePersistentCLI] Failed to start session";
        cleanup();
        return false;
    }

    m_started = true;

    // Wait for CLI to initialize
    QThread::msleep(500);

    return true;
}

void ClaudePersistentCLISession::cleanup()
{
    if (m_session) {
        if (m_session->isRunning()) {
            // Try graceful exit first
            m_session->write("/exit\r\n");
            QThread::msleep(100);
            m_session->terminate();
        }
        delete m_session;
        m_session = nullptr;
    }
    m_started = false;
    m_trustAccepted = false;
    m_binaryPath.clear();
    m_env.clear();
}

QByteArray ClaudePersistentCLISession::readChunk()
{
    if (!m_session) return {};
    return m_session->readOutput(50);
}

void ClaudePersistentCLISession::drainOutput()
{
    readChunk();
}

bool ClaudePersistentCLISession::send(const QString& text)
{
    if (!m_session || !m_session->isRunning()) return false;
    return m_session->write(text.toUtf8());
}

bool ClaudePersistentCLISession::handleTrustPrompt(const QString& output)
{
    QString lower = output.toLower();
    if (lower.contains(QStringLiteral("do you trust")) &&
        !lower.contains(QStringLiteral("current session"))) {
        qDebug() << "[ClaudePersistentCLI] Detected trust prompt, sending 'y'";
        send("y\r\n");
        m_trustAccepted = true;
        QThread::msleep(300);
        return true;
    }
    return false;
}

bool ClaudePersistentCLISession::containsUsageMarker(const QString& text)
{
    QString lower = text.toLower();
    QString compact = lower;
    compact.remove(QRegularExpression("\\s"));

    return compact.contains("currentsession") ||
           compact.contains("currentweek") ||
           lower.contains('%');
}

bool ClaudePersistentCLISession::containsStatusMarker(const QString& text)
{
    QString lower = text.toLower();

    return lower.contains("account:") ||
           lower.contains("email:") ||
           lower.contains("organization:") ||
           lower.contains("login method:") ||
           lower.contains("claude max") ||
           lower.contains("claude pro") ||
           lower.contains("claude team") ||
           lower.contains("claude enterprise") ||
           lower.contains("claude ultra");
}

bool ClaudePersistentCLISession::containsErrorMarker(const QString& text)
{
    QString lower = text.toLower();

    return lower.contains("token_expired") ||
           lower.contains("authentication_error") ||
           lower.contains("not logged in") ||
           lower.contains("failed to load usage data");
}

ClaudePersistentCLISession::CaptureResult ClaudePersistentCLISession::captureUsage(
    const QString& binary,
    int cols,
    int rows,
    int timeoutMs,
    const QHash<QString, QString>& env)
{
    CaptureResult result;
    result.success = false;

    if (!ensureStarted(binary, cols, rows, env)) {
        result.errorMessage = "Failed to start persistent Claude CLI session";
        return result;
    }

    drainOutput();

    QByteArray buffer;
    QDateTime deadline = QDateTime::currentDateTimeUtc().addMSecs(timeoutMs);
    bool sentCommand = false;
    bool sawOutput = false;
    int enterRetries = 0;
    int resendRetries = 0;
    QDateTime lastEnter = QDateTime::currentDateTimeUtc().addSecs(-10);
    QDateTime commandSentAt;

    while (QDateTime::currentDateTimeUtc() < deadline) {
        QByteArray newData = readChunk();
        if (!newData.isEmpty()) {
            buffer.append(newData);
        }

        QString bufferText = QString::fromUtf8(buffer);

        // Handle trust prompt
        if (!m_trustAccepted) {
            handleTrustPrompt(bufferText);
        }

        // Check for errors
        if (containsErrorMarker(bufferText)) {
            result.success = true;
            result.output = bufferText;
            return result;
        }

        // Check for usage markers
        if (containsUsageMarker(bufferText)) {
            sawOutput = true;
        }

        // Send /usage command
        if (!sentCommand) {
            send("/usage\r");
            sentCommand = true;
            commandSentAt = QDateTime::currentDateTimeUtc();
            lastEnter = QDateTime::currentDateTimeUtc();
            QThread::msleep(100);
            continue;
        }

        // Retry with enter if no response
        if (sentCommand && !sawOutput) {
            if (lastEnter.secsTo(QDateTime::currentDateTimeUtc()) >= 1.0 && enterRetries < 5) {
                send("\r");
                enterRetries++;
                lastEnter = QDateTime::currentDateTimeUtc();
                QThread::msleep(50);
                continue;
            }
            if (commandSentAt.secsTo(QDateTime::currentDateTimeUtc()) >= 2.5 && resendRetries < 2) {
                send("/usage\r");
                resendRetries++;
                buffer.clear();
                sawOutput = false;
                commandSentAt = QDateTime::currentDateTimeUtc();
                lastEnter = QDateTime::currentDateTimeUtc();
                QThread::msleep(100);
                continue;
            }
        }

        // Wait for output to settle after seeing markers
        if (sawOutput) {
            QDateTime settleStart = QDateTime::currentDateTimeUtc();
            while (QDateTime::currentDateTimeUtc().msecsTo(settleStart.addMSecs(900)) > 0) {
                QByteArray more = readChunk();
                if (!more.isEmpty()) {
                    buffer.append(more);
                }
                QThread::msleep(50);
            }
            break;
        }

        if (!m_session || !m_session->isRunning()) {
            result.errorMessage = "Claude CLI session exited unexpectedly";
            cleanup();
            return result;
        }

        QThread::msleep(50);
    }

    if (buffer.isEmpty()) {
        result.errorMessage = "Timed out waiting for usage output";
        return result;
    }

    result.success = true;
    result.output = QString::fromUtf8(buffer);
    return result;
}

ClaudePersistentCLISession::CaptureResult ClaudePersistentCLISession::captureStatus(
    const QString& binary,
    int cols,
    int rows,
    int timeoutMs,
    const QHash<QString, QString>& env)
{
    CaptureResult result;
    result.success = false;

    if (!ensureStarted(binary, cols, rows, env)) {
        result.errorMessage = "Failed to start persistent Claude CLI session";
        return result;
    }

    drainOutput();

    QByteArray buffer;
    QDateTime deadline = QDateTime::currentDateTimeUtc().addMSecs(timeoutMs);
    bool sentCommand = false;
    bool sawOutput = false;
    int enterRetries = 0;
    QDateTime lastEnter = QDateTime::currentDateTimeUtc().addSecs(-10);
    QDateTime commandSentAt;

    while (QDateTime::currentDateTimeUtc() < deadline) {
        QByteArray newData = readChunk();
        if (!newData.isEmpty()) {
            buffer.append(newData);
        }

        QString bufferText = QString::fromUtf8(buffer);

        // Handle trust prompt
        if (!m_trustAccepted) {
            handleTrustPrompt(bufferText);
        }

        // Check for status markers
        if (containsStatusMarker(bufferText)) {
            sawOutput = true;
        }

        // Send /status command
        if (!sentCommand) {
            send("/status\r");
            sentCommand = true;
            commandSentAt = QDateTime::currentDateTimeUtc();
            lastEnter = QDateTime::currentDateTimeUtc();
            QThread::msleep(100);
            continue;
        }

        // Retry with enter if no response
        if (sentCommand && !sawOutput && lastEnter.secsTo(QDateTime::currentDateTimeUtc()) >= 1.0 && enterRetries < 5) {
            send("\r");
            enterRetries++;
            lastEnter = QDateTime::currentDateTimeUtc();
            QThread::msleep(50);
        }

        // Wait for output to settle
        if (sawOutput) {
            QDateTime settleStart = QDateTime::currentDateTimeUtc();
            while (QDateTime::currentDateTimeUtc().msecsTo(settleStart.addMSecs(600)) > 0) {
                QByteArray more = readChunk();
                if (!more.isEmpty()) {
                    buffer.append(more);
                }
                QThread::msleep(50);
            }
            break;
        }

        if (!m_session || !m_session->isRunning()) {
            result.errorMessage = "Claude CLI session exited unexpectedly";
            cleanup();
            return result;
        }

        QThread::msleep(50);
    }

    if (buffer.isEmpty()) {
        result.errorMessage = "Timed out waiting for status output";
        return result;
    }

    result.success = true;
    result.output = QString::fromUtf8(buffer);
    return result;
}

ClaudePersistentCLISession::UsageStatusResult ClaudePersistentCLISession::captureUsageAndStatus(
    const QString& binary,
    int cols,
    int rows,
    int totalTimeoutMs,
    const QHash<QString, QString>& env)
{
    UsageStatusResult result;

    const int usageTimeout = qMax(3000, (totalTimeoutMs * 2) / 3);
    const int statusTimeout = qMax(2000, totalTimeoutMs - usageTimeout);

    // Capture usage first
    auto usageResult = captureUsage(binary, cols, rows, usageTimeout, env);
    if (!usageResult.success) {
        result.errorMessage = usageResult.errorMessage;
        return result;
    }

    result.usageOutput = usageResult.output;

    // Then capture status (reuses same session)
    auto statusResult = captureStatus(binary, cols, rows, statusTimeout, env);
    if (statusResult.success) {
        result.statusOutput = statusResult.output;
    } else {
        qDebug() << "[ClaudePersistentCLI] Status capture failed; continuing with usage output:"
                 << statusResult.errorMessage;
    }

    result.success = true;
    return result;
}

void ClaudePersistentCLISession::reset()
{
    cleanup();
}
