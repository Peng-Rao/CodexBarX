#include "app/UiFreezeWatchdog.h"

#include <QtTest/QtTest>

class UiFreezeWatchdogTest : public QObject {
    Q_OBJECT

private slots:
    void phaseScopeRestoresNestedPhases();
};

void UiFreezeWatchdogTest::phaseScopeRestoresNestedPhases()
{
    UiFreezeWatchdog::setCurrentPhase(QStringLiteral("idle"));
    QCOMPARE(UiFreezeWatchdog::currentPhase(), QStringLiteral("idle"));

    {
        UiFreezeWatchdog::PhaseScope outer(QStringLiteral("tray.load"));
        QCOMPARE(UiFreezeWatchdog::currentPhase(), QStringLiteral("tray.load"));

        {
            UiFreezeWatchdog::PhaseScope inner(QStringLiteral("usage.open"));
            QCOMPARE(UiFreezeWatchdog::currentPhase(), QStringLiteral("usage.open"));
        }

        QCOMPARE(UiFreezeWatchdog::currentPhase(), QStringLiteral("tray.load"));
    }

    QCOMPARE(UiFreezeWatchdog::currentPhase(), QStringLiteral("idle"));
}

QTEST_MAIN(UiFreezeWatchdogTest)

#include "tst_UiFreezeWatchdog.moc"
