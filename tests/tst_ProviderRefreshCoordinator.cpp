#include "app/ProviderRefreshCoordinator.h"

#include <QtTest/QtTest>

class tst_ProviderRefreshCoordinator : public QObject {
    Q_OBJECT

private slots:
    void applyRefreshResultTracksDashboardAndFetchAttempts();
    void failedRefreshClearsAttemptsForProvider();
};

void tst_ProviderRefreshCoordinator::applyRefreshResultTracksDashboardAndFetchAttempts()
{
    ProviderRefreshCoordinator coordinator;

    ProviderFetchResult result;
    result.success = true;
    result.usage.updatedAt = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
    RateWindow primary;
    primary.usedPercent = 33.0;
    result.usage.primary = primary;

    ProviderFetchAttempt attempt;
    attempt.strategyID = QStringLiteral("cli");
    attempt.strategyKindLabel = QStringLiteral("CLI");
    attempt.available = true;
    attempt.success = true;
    result.attempts.append(attempt);

    QJsonObject dashboard;
    dashboard.insert(QStringLiteral("visibility"), QStringLiteral("attached"));
    result.dashboard = dashboard;

    coordinator.applyRefreshResult(QStringLiteral("codex"), result);

    QCOMPARE(coordinator.snapshot(QStringLiteral("codex")).primary->usedPercent, 33.0);
    QCOMPARE(coordinator.fetchAttempts(QStringLiteral("codex")).size(), 1);
    QCOMPARE(coordinator.fetchAttempts(QStringLiteral("codex")).first().strategyID, QStringLiteral("cli"));
    QCOMPARE(coordinator.dashboardData(QStringLiteral("codex")).value(QStringLiteral("visibility")).toString(),
             QStringLiteral("attached"));
}

void tst_ProviderRefreshCoordinator::failedRefreshClearsAttemptsForProvider()
{
    ProviderRefreshCoordinator coordinator;

    ProviderFetchResult success;
    success.success = true;
    ProviderFetchAttempt attempt;
    attempt.strategyID = QStringLiteral("cli");
    success.attempts.append(attempt);
    coordinator.applyRefreshResult(QStringLiteral("codex"), success);
    QCOMPARE(coordinator.fetchAttempts(QStringLiteral("codex")).size(), 1);

    ProviderFetchResult failure;
    failure.success = false;
    failure.errorMessage = QStringLiteral("network down");
    coordinator.applyRefreshResult(QStringLiteral("codex"), failure);

    QVERIFY(coordinator.fetchAttempts(QStringLiteral("codex")).isEmpty());
    QCOMPARE(coordinator.error(QStringLiteral("codex")), QStringLiteral("network down"));
}

QTEST_MAIN(tst_ProviderRefreshCoordinator)

#include "tst_ProviderRefreshCoordinator.moc"
