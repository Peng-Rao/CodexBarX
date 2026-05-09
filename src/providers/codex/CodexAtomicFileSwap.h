#pragma once

#include <QString>
#include <QByteArray>
#include <optional>

/**
 * @brief CodexAtomicFileSwap provides atomic file write operations.
 *
 * On Windows, this uses MoveFileExW with MOVEFILE_REPLACE_EXISTING to
 * atomically replace the target file with staged content.
 *
 * Usage:
 *   CodexAtomicFileSwap swapper(targetPath);
 *   if (swapper.stageFile(data)) {
 *       if (swapper.commit()) {
 *           // success
 *       } else {
 *           swapper.rollback();
 *       }
 *   }
 */
class CodexAtomicFileSwap {
public:
    explicit CodexAtomicFileSwap(const QString& targetPath);
    ~CodexAtomicFileSwap();

    /**
     * @brief Stage data to a temporary file.
     * @param data The data to write.
     * @return true if staging succeeded.
     */
    bool stageFile(const QByteArray& data);

    /**
     * @brief Atomically commit the staged file to target.
     * @return true if commit succeeded.
     */
    bool commit();

    /**
     * @brief Rollback by removing the staged file.
     */
    void rollback();

    /**
     * @brief Get the last error message.
     */
    QString errorMessage() const { return m_errorMessage; }

    /**
     * @brief Check if a staging file exists.
     */
    bool hasStagedFile() const { return !m_stagedPath.isEmpty(); }

private:
    QString m_targetPath;
    QString m_stagedPath;
    QString m_errorMessage;

    bool setFilePermissions(const QString& path);
    QString generateStagedPath() const;
};
