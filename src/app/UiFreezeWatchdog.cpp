#include "UiFreezeWatchdog.h"

#include <QDebug>
#include <QtGlobal>

namespace {
thread_local QString t_currentPhase = QStringLiteral("idle");
}

UiFreezeWatchdog::UiFreezeWatchdog(QObject* parent)
    : QObject(parent)
{
    m_timer.setSingleShot(false);
    connect(&m_timer, &QTimer::timeout, this, &UiFreezeWatchdog::tick);
}

void UiFreezeWatchdog::start(int intervalMs, int warnBlockedMs, int errorBlockedMs)
{
    m_intervalMs = qMax(1, intervalMs);
    m_warnBlockedMs = qMax(1, warnBlockedMs);
    m_errorBlockedMs = qMax(m_warnBlockedMs, errorBlockedMs);
    m_elapsed.restart();
    m_lastElapsedMs = m_elapsed.elapsed();
    m_timer.start(m_intervalMs);
}

void UiFreezeWatchdog::stop()
{
    m_timer.stop();
    m_lastElapsedMs = 0;
}

QString UiFreezeWatchdog::currentPhase()
{
    return t_currentPhase.isEmpty() ? QStringLiteral("idle") : t_currentPhase;
}

void UiFreezeWatchdog::setCurrentPhase(const QString& phase)
{
    t_currentPhase = phase.trimmed().isEmpty() ? QStringLiteral("idle") : phase.trimmed();
}

UiFreezeWatchdog::PhaseScope::PhaseScope(const QString& phase)
    : m_previousPhase(UiFreezeWatchdog::currentPhase())
{
    UiFreezeWatchdog::setCurrentPhase(phase);
}

UiFreezeWatchdog::PhaseScope::~PhaseScope()
{
    UiFreezeWatchdog::setCurrentPhase(m_previousPhase);
}

void UiFreezeWatchdog::tick()
{
    if (!m_elapsed.isValid()) {
        m_elapsed.start();
        m_lastElapsedMs = m_elapsed.elapsed();
        return;
    }

    const qint64 now = m_elapsed.elapsed();
    const qint64 deltaMs = now - m_lastElapsedMs;
    m_lastElapsedMs = now;

    const qint64 blockedMs = qMax<qint64>(0, deltaMs - m_intervalMs);
    if (blockedMs < m_warnBlockedMs) {
        return;
    }

    const char* level = blockedMs >= m_errorBlockedMs ? "error" : "warn";
    qWarning().noquote()
        << "[UIFreezeWatchdog]"
        << "level=" << level
        << "blockedMs=" << blockedMs
        << "tickDeltaMs=" << deltaMs
        << "phase=" << currentPhase();
}
