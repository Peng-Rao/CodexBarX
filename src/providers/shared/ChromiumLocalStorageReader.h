#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct ChromiumStorageEntry {
    QString key;
    QString value;
};

class ChromiumLocalStorageReader {
public:
    static constexpr int kMaxFileSize = 32 * 1024 * 1024;

    static QVector<ChromiumStorageEntry> readEntries(
        const QString& origin,
        const QString& levelDBDir,
        const QStringList& targetKeys);

    static QVector<ChromiumStorageEntry> readTextEntries(
        const QString& levelDBDir,
        const QStringList& targetKeys);

private:
    struct CandidateFile {
        QString path;
        qint64 modifiedMs = 0;
    };

    static QVector<CandidateFile> enumerateFiles(const QString& levelDBDir);
    static QByteArray readFileBytes(const QString& filePath);
    static QVector<ChromiumStorageEntry> extractOriginAware(
        const QByteArray& data,
        const QString& origin,
        const QStringList& targetKeys);
    static QVector<ChromiumStorageEntry> extractTextFallback(
        const QByteArray& data,
        const QStringList& targetKeys);
    static QString decodeLatin1PlusUtf16LE(const QByteArray& data);
};
