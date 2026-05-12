#include "../src/providers/claude/ClaudePeakHours.h"
#include <QtTest/QtTest>

class tst_ClaudePeakHours : public QObject {
    Q_OBJECT

private slots:
    void detectsPeakHours();
    void detectsOffPeakBeforeWindow();
    void detectsOffPeakAfterWindow();
    void detectsWeekendOffPeak();
    void calculatesMinutesUntilEndOfPeak();
    void calculatesMinutesUntilNextPeak();
    void labelContainsPeakOrOffPeak();
};

void tst_ClaudePeakHours::detectsPeakHours()
{
    // Monday 10:00 AM ET = peak
    QDateTime mondayPeak(QDate(2026, 5, 11), QTime(10, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(mondayPeak);
    QVERIFY(status.isPeak);
}

void tst_ClaudePeakHours::detectsOffPeakBeforeWindow()
{
    // Monday 6:00 AM ET = off-peak (before peak starts)
    QDateTime mondayBefore(QDate(2026, 5, 11), QTime(6, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(mondayBefore);
    QVERIFY(!status.isPeak);
}

void tst_ClaudePeakHours::detectsOffPeakAfterWindow()
{
    // Monday 15:00 ET = off-peak (after peak ends)
    QDateTime mondayAfter(QDate(2026, 5, 11), QTime(15, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(mondayAfter);
    QVERIFY(!status.isPeak);
}

void tst_ClaudePeakHours::detectsWeekendOffPeak()
{
    // Saturday 10:00 AM ET = off-peak (weekend)
    QDateTime saturday(QDate(2026, 5, 9), QTime(10, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(saturday);
    QVERIFY(!status.isPeak);

    // Sunday 10:00 AM ET = off-peak (weekend)
    QDateTime sunday(QDate(2026, 5, 10), QTime(10, 0), QTimeZone("America/New_York"));
    status = ClaudePeakHours::status(sunday);
    QVERIFY(!status.isPeak);
}

void tst_ClaudePeakHours::calculatesMinutesUntilEndOfPeak()
{
    // Monday 10:00 AM ET, peak ends at 14:00 = 4 hours = 240 minutes
    QDateTime mondayPeak(QDate(2026, 5, 11), QTime(10, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(mondayPeak);
    QCOMPARE(status.minutesUntilChange, 240);
}

void tst_ClaudePeakHours::calculatesMinutesUntilNextPeak()
{
    // Monday 6:00 AM ET, peak starts at 8:00 = 2 hours = 120 minutes
    QDateTime mondayBefore(QDate(2026, 5, 11), QTime(6, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(mondayBefore);
    QCOMPARE(status.minutesUntilChange, 120);
}

void tst_ClaudePeakHours::labelContainsPeakOrOffPeak()
{
    QDateTime mondayPeak(QDate(2026, 5, 11), QTime(10, 0), QTimeZone("America/New_York"));
    ClaudePeakStatus status = ClaudePeakHours::status(mondayPeak);
    QVERIFY(status.label.contains("Peak"));
    QVERIFY(!status.label.contains("Off-peak"));

    QDateTime mondayOffPeak(QDate(2026, 5, 11), QTime(6, 0), QTimeZone("America/New_York"));
    status = ClaudePeakHours::status(mondayOffPeak);
    QVERIFY(status.label.contains("Off-peak"));
}

QTEST_MAIN(tst_ClaudePeakHours)
#include "tst_ClaudePeakHours.moc"
