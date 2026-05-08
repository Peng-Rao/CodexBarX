#include "ChromiumLocalStorageReader.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringConverter>

#include <algorithm>
#include <cstring>

QVector<ChromiumLocalStorageReader::CandidateFile> ChromiumLocalStorageReader::enumerateFiles(const QString& levelDBDir)
{
    QVector<CandidateFile> files;
    QDir dir(levelDBDir);
    if (!dir.exists()) return files;

    const auto entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const auto& info : entries) {
        const QString ext = info.suffix().toLower();
        if (ext == QStringLiteral("ldb") || ext == QStringLiteral("log")) {
            CandidateFile cf;
            cf.path = QDir::fromNativeSeparators(info.absoluteFilePath());
            cf.modifiedMs = info.lastModified().toMSecsSinceEpoch();
            files.append(cf);
        }
    }

    std::sort(files.begin(), files.end(), [](const CandidateFile& a, const CandidateFile& b) {
        return a.modifiedMs > b.modifiedMs;
    });

    return files;
}

QByteArray ChromiumLocalStorageReader::readFileBytes(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    const qint64 size = file.size();
    if (size <= 0 || size > kMaxFileSize) return {};
    return file.readAll();
}

QString ChromiumLocalStorageReader::decodeLatin1PlusUtf16LE(const QByteArray& data)
{
    QString result;
    for (int i = 0; i < data.size(); ++i) {
        const quint8 byte = static_cast<quint8>(data.at(i));
        if (byte >= 0x20 && byte <= 0x7E) {
            result.append(QChar(byte));
        } else if (byte == 0x0A || byte == 0x0D || byte == 0x09) {
            result.append(QChar::Space);
        } else {
            result.append(QChar::Space);
        }
    }
    return result;
}

static int indexOfInBytes(const QByteArray& data, const QByteArray& pattern, int from = 0)
{
    if (pattern.isEmpty()) return -1;
    const char* raw = data.constData();
    const int size = data.size();
    const int patSize = pattern.size();
    for (int i = from; i <= size - patSize; ++i) {
        if (memcmp(raw + i, pattern.constData(), patSize) == 0) {
            return i;
        }
    }
    return -1;
}

static QByteArray bytesFromUTF16LE(const QByteArray& data)
{
    QByteArray result;
    result.reserve(data.size() / 2);
    for (int i = 0; i + 1 < data.size(); i += 2) {
        quint8 lo = static_cast<quint8>(data.at(i));
        quint8 hi = static_cast<quint8>(data.at(i + 1));
        quint16 ch = static_cast<quint16>(lo) | (static_cast<quint16>(hi) << 8);
        if (ch <= 0x7F) {
            result.append(static_cast<char>(ch));
        }
    }
    return result;
}

static bool isValueChar(quint16 ch)
{
    return ch >= 0x20 && ch <= 0x7E;
}

static bool looksLikeUTF16LEValue(const QByteArray& data, int start)
{
    int pairs = 0;
    int asciiPairs = 0;
    constexpr int kProbeBytes = 32;
    for (int i = start; i + 1 < data.size() && (i - start) < kProbeBytes; i += 2) {
        const quint8 lo = static_cast<quint8>(data.at(i));
        const quint8 hi = static_cast<quint8>(data.at(i + 1));
        if (lo == 0 && hi == 0) break;
        ++pairs;
        if (hi == 0 && isValueChar(lo)) {
            ++asciiPairs;
        }
    }
    return pairs >= 3 && asciiPairs == pairs;
}

static QByteArray extractValueBytes(const QByteArray& data, int keyEnd)
{
    if (keyEnd >= data.size()) return {};
    int start = keyEnd;
    while (start < data.size()) {
        const quint8 b = static_cast<quint8>(data.at(start));
        if (b == 0x00 || b == 0x01 || b == 0x08 || b == 0x12 || b == 0x1A) {
            ++start;
            continue;
        }
        break;
    }
    if (start >= data.size()) return {};

    if (looksLikeUTF16LEValue(data, start)) {
        int end = start;
        constexpr int kMaxValueWindow = 4096;
        for (int i = start; i + 1 < data.size() && (i - start) < kMaxValueWindow; i += 2) {
            const quint8 lo = static_cast<quint8>(data.at(i));
            const quint8 hi = static_cast<quint8>(data.at(i + 1));
            const quint16 ch = static_cast<quint16>(lo) | (static_cast<quint16>(hi) << 8);
            if (ch == 0 || !isValueChar(ch)) break;
            end = i + 2;
        }
        return data.mid(start, end - start);
    }

    int end = start;
    constexpr int kMaxValueWindow = 4096;
    for (int i = start; i < data.size() && (i - start) < kMaxValueWindow; ++i) {
        const quint8 b = static_cast<quint8>(data.at(i));
        if (b >= 0x20 && b <= 0x7E) {
            end = i + 1;
        } else if (b == 0x22 || b == 0x27 || b == 0x2D || b == 0x5F || b == 0x2E || b == 0x24) {
            end = i + 1;
        } else if (b == 0x00 && (i - start) >= 4) {
            break;
        } else if (b < 0x20 && b != 0x09 && b != 0x0A && b != 0x0D) {
            if ((i - start) >= 4) break;
        }
    }

    return data.mid(start, end - start);
}

static QString decodeValueBytes(const QByteArray& valueBytes)
{
    if (valueBytes.isEmpty()) return {};

    int pairs = 0;
    int zeroHighPairs = 0;
    for (int i = 0; i + 1 < valueBytes.size(); i += 2) {
        ++pairs;
        if (static_cast<quint8>(valueBytes.at(i + 1)) == 0) {
            ++zeroHighPairs;
        }
    }

    if (pairs >= 2 && zeroHighPairs == pairs) {
        QStringDecoder decoder(QStringConverter::Utf16LE);
        const QString decoded = decoder(valueBytes);
        return decoded.trimmed();
    }

    return QString::fromUtf8(valueBytes).trimmed();
}

QVector<ChromiumStorageEntry> ChromiumLocalStorageReader::extractOriginAware(
    const QByteArray& data,
    const QString& origin,
    const QStringList& targetKeys)
{
    QVector<ChromiumStorageEntry> entries;
    const QByteArray originBytes = origin.toUtf8();
    const int originLen = originBytes.size();

    int pos = 0;
    while (true) {
        const int originIdx = indexOfInBytes(data, originBytes, pos);
        if (originIdx < 0) break;
        pos = originIdx + originLen;

        constexpr int kSearchWindow = 8192;
        const int windowStart = qMax(0, originIdx - kSearchWindow / 2);
        const int windowEnd = qMin(data.size(), originIdx + kSearchWindow);

        for (const auto& key : targetKeys) {
            const QByteArray keyBytes = key.toUtf8();
            const int keyIdx = indexOfInBytes(data, keyBytes, windowStart);
            if (keyIdx < 0 || keyIdx >= windowEnd) continue;
            if (keyIdx + keyBytes.size() >= data.size()) continue;

            const int keyEnd = keyIdx + keyBytes.size();
            const QByteArray valueBytes = extractValueBytes(data, keyEnd);
            if (valueBytes.isEmpty()) continue;

            const QString value = decodeValueBytes(valueBytes);
            if (!value.isEmpty()) {
                entries.append({key, value});
            }
        }
    }

    return entries;
}

QVector<ChromiumStorageEntry> ChromiumLocalStorageReader::extractTextFallback(
    const QByteArray& data,
    const QStringList& targetKeys)
{
    QVector<ChromiumStorageEntry> entries;
    const QString text = decodeLatin1PlusUtf16LE(data);
    const QByteArray utf16ASCII = bytesFromUTF16LE(data);
    const QString text2 = QString::fromUtf8(utf16ASCII);

    auto tryExtract = [&](const QString& haystack) {
        for (const auto& key : targetKeys) {
            const int keyIdx = haystack.indexOf(key, 0, Qt::CaseInsensitive);
            if (keyIdx < 0) continue;

            int valStart = keyIdx + key.size();
            while (valStart < haystack.size() && haystack.at(valStart).isSpace()) {
                ++valStart;
            }
            if (valStart >= haystack.size()) continue;

            int valEnd = valStart;
            for (int i = valStart; i < haystack.size() && (i - valStart) < 2048; ++i) {
                const QChar ch = haystack.at(i);
                if (ch.isLetterOrNumber() || ch == '$' || ch == '-' || ch == '_' ||
                    ch == '.' || ch == '/' || ch == '=' || ch == '+' || ch == ':') {
                    valEnd = i + 1;
                } else if (ch == '"' || ch == '\'' || ch.isSpace()) {
                    valEnd = i + 1;
                } else {
                    break;
                }
            }

            if (valEnd > valStart) {
                entries.append({key, haystack.mid(valStart, valEnd - valStart).trimmed()});
            }
        }
    };

    tryExtract(text);
    tryExtract(text2);

    return entries;
}

QVector<ChromiumStorageEntry> ChromiumLocalStorageReader::readEntries(
    const QString& origin,
    const QString& levelDBDir,
    const QStringList& targetKeys)
{
    const auto files = enumerateFiles(levelDBDir);
    QVector<ChromiumStorageEntry> results;

    for (const auto& cf : files) {
        const QByteArray bytes = readFileBytes(cf.path);
        if (bytes.isEmpty()) continue;

        auto originEntries = extractOriginAware(bytes, origin, targetKeys);
        for (const auto& entry : originEntries) {
            bool found = false;
            for (const auto& existing : results) {
                if (existing.key == entry.key) { found = true; break; }
            }
            if (!found) results.append(entry);
        }
    }

    return results;
}

QVector<ChromiumStorageEntry> ChromiumLocalStorageReader::readTextEntries(
    const QString& levelDBDir,
    const QStringList& targetKeys)
{
    const auto files = enumerateFiles(levelDBDir);
    QVector<ChromiumStorageEntry> results;

    for (const auto& cf : files) {
        const QByteArray bytes = readFileBytes(cf.path);
        if (bytes.isEmpty()) continue;

        auto textEntries = extractTextFallback(bytes, targetKeys);
        for (const auto& entry : textEntries) {
            bool found = false;
            for (const auto& existing : results) {
                if (existing.key == entry.key) { found = true; break; }
            }
            if (!found) results.append(entry);
        }
    }

    return results;
}
