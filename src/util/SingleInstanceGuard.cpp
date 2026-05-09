#include "SingleInstanceGuard.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#endif

SingleInstanceGuard::SingleInstanceGuard(const QString& key)
{
#ifdef Q_OS_WIN
    if (key.isEmpty()) return;

    const std::wstring name = key.toStdWString();
    HANDLE handle = CreateMutexW(nullptr, FALSE, name.c_str());
    if (!handle) {
        m_alreadyRunning = GetLastError() == ERROR_ACCESS_DENIED;
        return;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(handle);
        m_alreadyRunning = true;
        return;
    }

    m_handle = handle;
    m_acquired = true;
#else
    // Unix/macOS: use file lock
    if (key.isEmpty()) return;

    // Create lock file in temp directory
    QString lockDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(lockDir);

    // Sanitize key for use as filename (remove potentially problematic chars)
    QString safeKey = key;
    safeKey.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_]")), QStringLiteral("_"));

    QString lockPath = lockDir + QDir::separator() + safeKey + QStringLiteral(".lock");

    int fd = open(lockPath.toUtf8().constData(), O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        m_alreadyRunning = false;
        m_acquired = false;
        return;
    }

    // Try to acquire exclusive lock (non-blocking)
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        m_alreadyRunning = true;
        m_acquired = false;
        return;
    }

    m_handle = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
    m_acquired = true;
#endif
}

SingleInstanceGuard::~SingleInstanceGuard()
{
    release();
}

void SingleInstanceGuard::release()
{
#ifdef Q_OS_WIN
    if (m_handle) {
        CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
#else
    if (m_handle) {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(m_handle));
        flock(fd, LOCK_UN);
        close(fd);
        m_handle = nullptr;
    }
#endif
    m_acquired = false;
}
