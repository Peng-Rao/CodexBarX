#include "app/ProviderUIService.h"

#include <QtTest/QtTest>

class tst_ProviderUIService : public QObject {
    Q_OBJECT

private slots:
    void snapshotDataBuildsFullCardMapAndCachesUntilInvalidated();
};

void tst_ProviderUIService::snapshotDataBuildsFullCardMapAndCachesUntilInvalidated()
{
    ProviderUIService service;
    service.setDisplayNameAccessor([](const QString& providerId) {
        return providerId == QStringLiteral("demo")
            ? QStringLiteral("Demo Provider")
            : providerId;
    });
    service.setErrorAccessor([](const QString& providerId) {
        return providerId == QStringLiteral("demo")
            ? QStringLiteral("temporary outage")
            : QString();
    });

    UsageSnapshot first;
    first.updatedAt = QDateTime::fromMSecsSinceEpoch(1000, Qt::UTC);
    RateWindow primary;
    primary.usedPercent = 25.0;
    primary.resetDescription = QStringLiteral("in 2h");
    first.primary = primary;

    const QVariantMap firstMap = service.snapshotData(QStringLiteral("demo"), first);
    QCOMPARE(firstMap.value(QStringLiteral("displayName")).toString(), QStringLiteral("Demo Provider"));
    QCOMPARE(firstMap.value(QStringLiteral("primaryUsed")).toDouble(), 25.0);
    QCOMPARE(firstMap.value(QStringLiteral("primaryRemaining")).toDouble(), 75.0);
    QCOMPARE(firstMap.value(QStringLiteral("primaryDisplayPercent")).toDouble(), 75.0);
    QCOMPARE(firstMap.value(QStringLiteral("primaryDisplayIsUsed")).toBool(), false);
    QCOMPARE(firstMap.value(QStringLiteral("primaryResetDesc")).toString(), QStringLiteral("in 2h"));
    QCOMPARE(firstMap.value(QStringLiteral("hasUsage")).toBool(), true);
    QCOMPARE(firstMap.value(QStringLiteral("error")).toString(), QStringLiteral("temporary outage"));

    UsageSnapshot second = first;
    second.primary->usedPercent = 90.0;
    const QVariantMap cachedMap = service.snapshotData(QStringLiteral("demo"), second);
    QCOMPARE(cachedMap.value(QStringLiteral("primaryUsed")).toDouble(), 25.0);

    service.invalidateSnapshotDataCache(QStringLiteral("demo"));
    const QVariantMap rebuiltMap = service.snapshotData(QStringLiteral("demo"), second);
    QCOMPARE(rebuiltMap.value(QStringLiteral("primaryUsed")).toDouble(), 90.0);
    QCOMPARE(rebuiltMap.value(QStringLiteral("primaryRemaining")).toDouble(), 10.0);
}

QTEST_MAIN(tst_ProviderUIService)

#include "tst_ProviderUIService.moc"
