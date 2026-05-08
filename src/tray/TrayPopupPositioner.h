#pragma once

#include <QRect>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

class TrayPopupPositioner {
public:
#ifdef Q_OS_WIN
    static QRect getTrayIconRect(HWND hwnd, UINT iconID);
#endif
    static QRect estimateTrayIconRect();

private:
    static QPoint getTaskbarEdge();
#ifdef Q_OS_WIN
    static DWORD getTaskbarPosition();
#endif
};
