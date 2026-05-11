#pragma once

#include <QVector>
#include <QString>
#include <QVariantList>

struct StorageComponentItem {
    QString path;
    qint64 bytes = 0;
    bool canCopy = true;
};

struct StorageCleanupSuggestion {
    QString title;
    QString path;
    qint64 bytes = 0;
    QString consequence;
};

struct ProviderStorageFootprint {
    QString providerId;
    qint64 totalBytes = 0;
    QVector<StorageComponentItem> components;
    QVector<StorageCleanupSuggestion> cleanupSuggestions;
    QStringList missingPaths;
    QStringList unreadablePaths;
};

class ProviderStorageScanner {
public:
    // Scan storage for a specific provider
    static ProviderStorageFootprint scanProvider(const QString& providerId);

    // Get candidate storage paths for a provider
    static QStringList candidatePaths(const QString& providerId);

    // Calculate directory size recursively
    static qint64 calculateDirectorySize(const QString& path);

    // Check if path exists
    static bool pathExists(const QString& path);

    // Generate cleanup suggestions based on component name patterns
    static QVector<StorageCleanupSuggestion> generateCleanupSuggestions(
        const QString& providerId,
        const QVector<StorageComponentItem>& components,
        const QStringList& rootPaths);
};
