#include "CodexAtomicFileSwap.h"

#include <QFile>
#include <QDir>
#include <QUuid>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

CodexAtomicFileSwap::CodexAtomicFileSwap(const QString& targetPath)
    : m_targetPath(targetPath)
{
}

CodexAtomicFileSwap::~CodexAtomicFileSwap()
{
    // Auto-cleanup staged file if not committed
    if (!m_stagedPath.isEmpty()) {
        QFile::remove(m_stagedPath);
    }
}

bool CodexAtomicFileSwap::stageFile(const QByteArray& data)
{
    m_stagedPath = generateStagedPath();
    m_errorMessage.clear();

    QFile file(m_stagedPath);
    if (!file.open(QIODevice::WriteOnly)) {
        m_errorMessage = QStringLiteral("Failed to create staged file: %1").arg(file.errorString());
        m_stagedPath.clear();
        return false;
    }

    qint64 written = file.write(data);
    file.close();

    if (written != data.size()) {
        m_errorMessage = QStringLiteral("Failed to write complete data to staged file");
        QFile::remove(m_stagedPath);
        m_stagedPath.clear();
        return false;
    }

    // Set file permissions (read/write for owner only on Unix-like systems)
    if (!setFilePermissions(m_stagedPath)) {
        m_errorMessage = QStringLiteral("Failed to set file permissions");
        QFile::remove(m_stagedPath);
        m_stagedPath.clear();
        return false;
    }

    return true;
}

bool CodexAtomicFileSwap::commit()
{
    if (m_stagedPath.isEmpty()) {
        m_errorMessage = QStringLiteral("No staged file to commit");
        return false;
    }

    m_errorMessage.clear();

    // Ensure target directory exists
    QFileInfo targetInfo(m_targetPath);
    QDir targetDir = targetInfo.absoluteDir();
    if (!targetDir.exists()) {
        if (!targetDir.mkpath(QStringLiteral("."))) {
            m_errorMessage = QStringLiteral("Failed to create target directory");
            return false;
        }
    }

#ifdef Q_OS_WIN
    // Use MoveFileExW with MOVEFILE_REPLACE_EXISTING for atomic replace on Windows
    QString nativeStagedPath = QDir::toNativeSeparators(m_stagedPath);
    QString nativeTargetPath = QDir::toNativeSeparators(m_targetPath);

    BOOL result = MoveFileExW(
        reinterpret_cast<LPCWSTR>(nativeStagedPath.utf16()),
        reinterpret_cast<LPCWSTR>(nativeTargetPath.utf16()),
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
    );

    if (!result) {
        DWORD error = GetLastError();
        m_errorMessage = QStringLiteral("MoveFileExW failed with error code %1").arg(error);
        return false;
    }
#else
    // Use rename() on Unix-like systems (atomic on same filesystem)
    if (rename(m_stagedPath.toUtf8().constData(), m_targetPath.toUtf8().constData()) != 0) {
        m_errorMessage = QStringLiteral("rename() failed: %1").arg(QString::fromLocal8Bit(strerror(errno)));
        return false;
    }
#endif

    // Clear staged path so destructor doesn't try to remove it
    m_stagedPath.clear();
    return true;
}

void CodexAtomicFileSwap::rollback()
{
    if (!m_stagedPath.isEmpty()) {
        QFile::remove(m_stagedPath);
        m_stagedPath.clear();
    }
    m_errorMessage.clear();
}

QString CodexAtomicFileSwap::generateStagedPath() const
{
    QFileInfo info(m_targetPath);
    QString dir = info.absolutePath();
    QString baseName = info.fileName();
    QString stagedName = QStringLiteral("%1.codexbarx-staged-%2")
        .arg(baseName)
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));

    return QDir(dir).filePath(stagedName);
}

bool CodexAtomicFileSwap::setFilePermissions(const QString& path)
{
#ifdef Q_OS_WIN
    // On Windows, use SetFileAttributes or _chmod
    // For simplicity, we'll use Qt's permission system
    QFile file(path);
    return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
#else
    // On Unix, set permissions to 0600 (read/write for owner only)
    QFile file(path);
    return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
#endif
}
