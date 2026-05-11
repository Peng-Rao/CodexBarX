#include "ProviderStorageScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <algorithm>

ProviderStorageFootprint ProviderStorageScanner::scanProvider(const QString& providerId)
{
    ProviderStorageFootprint result;
    result.providerId = providerId;

    QStringList paths = candidatePaths(providerId);
    if (paths.isEmpty()) {
        return result;
    }

    for (const QString& path : paths) {
        if (!pathExists(path)) {
            result.missingPaths.append(path);
            continue;
        }

        QDir dir(path);
        if (!dir.exists()) {
            result.unreadablePaths.append(path);
            continue;
        }

        // Scan immediate children
        QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            it.next();
            QFileInfo info = it.fileInfo();

            StorageComponentItem item;
            item.path = info.absoluteFilePath();
            item.canCopy = true;

            if (info.isDir()) {
                item.bytes = calculateDirectorySize(info.absoluteFilePath());
            } else {
                item.bytes = info.size();
            }

            if (item.bytes > 0) {
                result.components.append(item);
                result.totalBytes += item.bytes;
            }
        }
    }

    // Sort by bytes descending
    std::sort(result.components.begin(), result.components.end(),
        [](const StorageComponentItem& a, const StorageComponentItem& b) {
            return a.bytes > b.bytes;
        });

    // Generate cleanup suggestions
    result.cleanupSuggestions = generateCleanupSuggestions(providerId, result.components, paths);

    return result;
}

QStringList ProviderStorageScanner::candidatePaths(const QString& providerId)
{
    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    auto homePath = [&](const QString& relative) -> QString {
        return home + "/" + relative;
    };

    if (providerId == "codex") {
        return {
            homePath(".codex"),
        };
    }
    if (providerId == "claude") {
        return {
            homePath(".claude"),
        };
    }
    if (providerId == "gemini") {
        return {
            homePath(".gemini"),
        };
    }
    if (providerId == "cursor") {
        return {
            homePath(".cursor"),
            homePath("AppData/Roaming/Cursor"),
        };
    }
    if (providerId == "copilot") {
        return {
            homePath(".copilot"),
        };
    }
    if (providerId == "windsurf") {
        return {
            homePath(".windsurf"),
        };
    }
    if (providerId == "deepseek") {
        return {
            homePath(".deepseek"),
        };
    }
    if (providerId == "opencode" || providerId == "opencodego") {
        return {
            homePath(".config/opencode"),
        };
    }

    return {};
}

qint64 ProviderStorageScanner::calculateDirectorySize(const QString& path)
{
    qint64 totalSize = 0;
    QDirIterator it(path, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QFileInfo info = it.fileInfo();
        if (info.isFile()) {
            totalSize += info.size();
        }
    }
    return totalSize;
}

bool ProviderStorageScanner::pathExists(const QString& path)
{
    return QFile::exists(path);
}

QVector<StorageCleanupSuggestion> ProviderStorageScanner::generateCleanupSuggestions(
    const QString& providerId,
    const QVector<StorageComponentItem>& components,
    const QStringList& rootPaths)
{
    QVector<StorageCleanupSuggestion> suggestions;

    auto isContainedInRoots = [&](const QString& path) -> bool {
        for (const QString& root : rootPaths) {
            if (path.startsWith(root)) {
                return true;
            }
        }
        return false;
    };

    for (const auto& comp : components) {
        if (!isContainedInRoots(comp.path)) {
            continue;
        }

        QString name = QFileInfo(comp.path).fileName();
        StorageCleanupSuggestion suggestion;
        suggestion.path = comp.path;
        suggestion.bytes = comp.bytes;

        if (providerId == "codex") {
            if (name == "sessions") {
                suggestion.title = QObject::tr("Manual cleanup: sessions");
                suggestion.consequence = QObject::tr("Clearing removes past Codex session history.");
            } else if (name == "archived_sessions") {
                suggestion.title = QObject::tr("Manual cleanup: archived sessions");
                suggestion.consequence = QObject::tr("Clearing removes archived Codex session history.");
            } else if (name == "cache" || name == "caches") {
                suggestion.title = QObject::tr("Manual cleanup: cache");
                suggestion.consequence = QObject::tr("Clearing removes provider-owned cached data.");
            } else if (name == "log" || name == "logs" || name == "debug") {
                suggestion.title = QObject::tr("Manual cleanup: logs");
                suggestion.consequence = QObject::tr("Clearing removes local diagnostic logs.");
            } else if (name == "file-history") {
                suggestion.title = QObject::tr("Manual cleanup: file history");
                suggestion.consequence = QObject::tr("Clearing removes local edit checkpoint history.");
            } else if (name == "tmp" || name == "temp") {
                suggestion.title = QObject::tr("Manual cleanup: temporary data");
                suggestion.consequence = QObject::tr("Clearing removes local temporary provider data.");
            } else {
                continue;
            }
        } else if (providerId == "claude") {
            if (name == "projects") {
                suggestion.title = QObject::tr("Manual cleanup: past sessions");
                suggestion.consequence = QObject::tr("Clearing removes past resume, continue, and rewind history.");
            } else if (name == "file-history") {
                suggestion.title = QObject::tr("Manual cleanup: file checkpoints");
                suggestion.consequence = QObject::tr("Clearing removes checkpoint restore data for previous edits.");
            } else if (name == "plans") {
                suggestion.title = QObject::tr("Manual cleanup: saved plans");
                suggestion.consequence = QObject::tr("Clearing removes old plan-mode files.");
            } else if (name == "debug") {
                suggestion.title = QObject::tr("Manual cleanup: debug logs");
                suggestion.consequence = QObject::tr("Clearing removes past debug logs.");
            } else if (name == "paste-cache" || name == "image-cache") {
                suggestion.title = QObject::tr("Manual cleanup: attachment cache");
                suggestion.consequence = QObject::tr("Clearing removes cached large pastes or attached images.");
            } else if (name == "todos") {
                suggestion.title = QObject::tr("Manual cleanup: legacy todos");
                suggestion.consequence = QObject::tr("Clearing removes legacy per-session task lists.");
            } else {
                continue;
            }
        } else {
            continue;
        }

        suggestions.append(suggestion);
    }

    // Sort by bytes descending
    std::sort(suggestions.begin(), suggestions.end(),
        [](const StorageCleanupSuggestion& a, const StorageCleanupSuggestion& b) {
            return a.bytes > b.bytes;
        });

    return suggestions;
}
