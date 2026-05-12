#pragma once

#include <QString>
#include <QDateTime>

struct ClaudePeakStatus {
    bool isPeak = false;
    QString label;
    int minutesUntilChange = 0;
};

class ClaudePeakHours {
public:
    static ClaudePeakStatus status(const QDateTime& at = QDateTime::currentDateTime());

private:
    static constexpr int PeakStartHour = 8;
    static constexpr int PeakEndHour = 14;

    static QTimeZone peakTimeZone();
    static int minutesUntilNextTransition(const QDateTime& etTime, bool currentlyPeak);
};
