#pragma once

#include <QObject>
#include <QHash>
#include <optional>

class ClaudeCLISession : public QObject {
    Q_OBJECT
public:
    struct CaptureResult {
        bool success = false;
        QString output;
        QString errorMessage;
    };

    explicit ClaudeCLISession(QObject* parent = nullptr);
    ~ClaudeCLISession() override;

    // Check if claude binary is available
    static bool isClaudeInstalled();
    static QString resolveBinaryPath();

    // Synchronous capture - designed for use in worker threads
    CaptureResult captureUsage(int timeoutMs = 20000);

    // Configuration
    void setEnvironment(const QHash<QString, QString>& env);
    void setTimeout(int timeoutMs);

private:
    CaptureResult captureInternal(const QString& subcommand, int timeoutMs);
    bool handleTrustPrompt(class ConPTYSession& session, const QString& output);

    QHash<QString, QString> m_env;
    int m_timeoutMs = 20000;
    int m_cols = 120;
    int m_rows = 30;
};
