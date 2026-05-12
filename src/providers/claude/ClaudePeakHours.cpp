#include "ClaudePeakHours.h"
#include <QTimeZone>

ClaudePeakStatus ClaudePeakHours::status(const QDateTime& at)
{
    ClaudePeakStatus result;

    QTimeZone et = peakTimeZone();
    QDateTime etTime = at.toTimeZone(et);
    QTime time = etTime.time();
    int dayOfWeek = etTime.date().dayOfWeek();

    bool isWeekday = dayOfWeek >= 1 && dayOfWeek <= 5;
    int hour = time.hour();
    bool inPeakWindow = hour >= PeakStartHour && hour < PeakEndHour;

    result.isPeak = isWeekday && inPeakWindow;
    result.minutesUntilChange = minutesUntilNextTransition(etTime, result.isPeak);

    int h = result.minutesUntilChange / 60;
    int m = result.minutesUntilChange % 60;

    if (result.isPeak) {
        if (h > 0) {
            result.label = QString("Peak · ends in %1h %2m").arg(h).arg(m);
        } else {
            result.label = QString("Peak · ends in %1m").arg(m);
        }
    } else {
        if (h > 0) {
            result.label = QString("Off-peak · peak in %1h %2m").arg(h).arg(m);
        } else {
            result.label = QString("Off-peak · peak in %1m").arg(m);
        }
    }

    return result;
}

QTimeZone ClaudePeakHours::peakTimeZone()
{
    return QTimeZone("America/New_York");
}

int ClaudePeakHours::minutesUntilNextTransition(const QDateTime& etTime, bool currentlyPeak)
{
    QTime time = etTime.time();
    int dayOfWeek = etTime.date().dayOfWeek();

    if (currentlyPeak) {
        QTime endTime(PeakEndHour, 0);
        int secs = time.secsTo(endTime);
        return secs / 60;
    }

    if (dayOfWeek >= 1 && dayOfWeek <= 5 && time.hour() < PeakStartHour) {
        QTime startTime(PeakStartHour, 0);
        int secs = time.secsTo(startTime);
        return secs / 60;
    }

    int daysUntilNextPeak = 0;

    if (dayOfWeek >= 1 && dayOfWeek <= 5 && time.hour() >= PeakEndHour) {
        daysUntilNextPeak = 1;
    } else if (dayOfWeek == 6) {
        daysUntilNextPeak = 2;
    } else if (dayOfWeek == 7) {
        daysUntilNextPeak = 1;
    }

    int minutesToday = (23 - time.hour()) * 60 + (60 - time.minute());
    int minutesFullDays = (daysUntilNextPeak - 1) * 24 * 60;
    int minutesUntilPeakStart = PeakStartHour * 60;

    return minutesToday + minutesFullDays + minutesUntilPeakStart;
}
