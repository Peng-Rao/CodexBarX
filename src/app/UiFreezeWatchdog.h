#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

class UiFreezeWatchdog : public QObject {
    Q_OBJECT

public:
    explicit UiFreezeWatchdog(QObject* parent = nullptr);

    void start(int intervalMs = 32, int warnBlockedMs = 120, int errorBlockedMs = 300);
    void stop();

    static QString currentPhase();
    static void setCurrentPhase(const QString& phase);

    class PhaseScope {
    public:
        explicit PhaseScope(const QString& phase);
        ~PhaseScope();

        PhaseScope(const PhaseScope&) = delete;
        PhaseScope& operator=(const PhaseScope&) = delete;

    private:
        QString m_previousPhase;
    };

private slots:
    void tick();

private:
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    qint64 m_lastElapsedMs = 0;
    int m_intervalMs = 32;
    int m_warnBlockedMs = 120;
    int m_errorBlockedMs = 300;
};
