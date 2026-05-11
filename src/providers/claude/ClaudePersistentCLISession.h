#pragma once

#include <QString>
#include <QHash>
#include <QByteArray>
#include <QMutex>
#include <optional>

class ConPTYSession;

class ClaudePersistentCLISession {
public:
    static ClaudePersistentCLISession& instance();

    struct CaptureResult {
        bool success = false;
        QString output;
        QString errorMessage;
    };

    struct UsageStatusResult {
        bool success = false;
        QString usageOutput;
        QString statusOutput;
        QString errorMessage;
    };

    // Capture /usage output
    CaptureResult captureUsage(
        const QString& binary,
        int cols,
        int rows,
        int timeoutMs,
        const QHash<QString, QString>& env);

    // Capture /status output
    CaptureResult captureStatus(
        const QString& binary,
        int cols,
        int rows,
        int timeoutMs,
        const QHash<QString, QString>& env);

    // Capture both /usage and /status in sequence
    UsageStatusResult captureUsageAndStatus(
        const QString& binary,
        int cols,
        int rows,
        int totalTimeoutMs,
        const QHash<QString, QString>& env);

    // Reset the session (kill and restart on next call)
    void reset();

    // Check if session is currently active
    bool isActive() const;

private:
    ClaudePersistentCLISession();
    ~ClaudePersistentCLISession();
    ClaudePersistentCLISession(const ClaudePersistentCLISession&) = delete;
    ClaudePersistentCLISession& operator=(const ClaudePersistentCLISession&) = delete;

    bool ensureStarted(
        const QString& binary,
        int cols,
        int rows,
        const QHash<QString, QString>& env);

    void cleanup();
    QByteArray readChunk();
    void drainOutput();
    bool send(const QString& text);
    bool handleTrustPrompt(const QString& output);

    ConPTYSession* m_session = nullptr;
    QString m_binaryPath;
    int m_cols = 0;
    int m_rows = 0;
    QHash<QString, QString> m_env;
    bool m_started = false;
    bool m_trustAccepted = false;

    // Output markers
    static bool containsUsageMarker(const QString& text);
    static bool containsStatusMarker(const QString& text);
    static bool containsErrorMarker(const QString& text);
};
