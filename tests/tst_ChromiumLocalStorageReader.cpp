#include <QtTest/QtTest>

#include "../src/providers/shared/ChromiumLocalStorageReader.h"

#include <QDir>
#include <QDateTime>
#include <QFile>
#include <QFileDevice>
#include <QTemporaryDir>

class tst_ChromiumLocalStorageReader : public QObject {
    Q_OBJECT

private slots:
    void emptyDirectory();
    void missingDirectory();
    void originAwareExtraction();
    void originAwareExtractionUtf16LE();
    void ignoresOtherOrigins();
    void textFallbackExtraction();
    void readsNewestFileFirst();
    void ignoresNonLevelDBFiles();

private:
    void writeFile(const QString& dirPath, const QString& name, const QByteArray& content);
    static QByteArray utf16LE(const QString& value);
    QString makeLevelDBDir(const QString& parentDir);
};

void tst_ChromiumLocalStorageReader::writeFile(const QString& dirPath, const QString& name,
                                               const QByteArray& content)
{
    QFile file(dirPath + QStringLiteral("/") + name);
    QVERIFY2(file.open(QIODevice::WriteOnly), qPrintable(file.errorString()));
    file.write(content);
    file.close();
}

QByteArray tst_ChromiumLocalStorageReader::utf16LE(const QString& value)
{
    return QByteArray(reinterpret_cast<const char*>(value.utf16()),
                      value.size() * static_cast<int>(sizeof(char16_t)));
}

QString tst_ChromiumLocalStorageReader::makeLevelDBDir(const QString& parentDir)
{
    QString path = parentDir + QStringLiteral("/leveldb");
    QDir().mkpath(path);
    return path;
}

void tst_ChromiumLocalStorageReader::emptyDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());
    auto entries = ChromiumLocalStorageReader::readEntries(
        QStringLiteral("https://windsurf.com"), levelDB,
        {QStringLiteral("devin_session_token")});
    QVERIFY(entries.isEmpty());

    auto textEntries = ChromiumLocalStorageReader::readTextEntries(levelDB,
        {QStringLiteral("devin_session_token")});
    QVERIFY(textEntries.isEmpty());
}

void tst_ChromiumLocalStorageReader::missingDirectory()
{
    auto entries = ChromiumLocalStorageReader::readEntries(
        QStringLiteral("https://windsurf.com"),
        QStringLiteral("/nonexistent/path/leveldb"),
        {QStringLiteral("key1")});
    QVERIFY(entries.isEmpty());
}

void tst_ChromiumLocalStorageReader::originAwareExtraction()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());

    // LDB file containing https://windsurf.com close to devin_session_token key and value
    QByteArray content;
    content.append("https://windsurf.com");
    content.append(QByteArray(128, '\0'));
    content.append("devin_session_token");
    content.append(QByteArray(4, '\0'));
    content.append("session-token-value-abc123");
    writeFile(levelDB, QStringLiteral("000005.ldb"), content);

    auto entries = ChromiumLocalStorageReader::readEntries(
        QStringLiteral("https://windsurf.com"), levelDB,
        {QStringLiteral("devin_session_token")});
    QVERIFY(!entries.isEmpty());
    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].key, QStringLiteral("devin_session_token"));
    QCOMPARE(entries[0].value, QStringLiteral("session-token-value-abc123"));
}

void tst_ChromiumLocalStorageReader::originAwareExtractionUtf16LE()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());

    QByteArray content;
    content.append("https://windsurf.com");
    content.append(QByteArray(128, '\0'));
    content.append("devin_session_token");
    content.append(QByteArray(4, '\0'));
    content.append(utf16LE(QStringLiteral("session-token-value-utf16")));
    content.append(QByteArray(8, '\0'));
    content.append("devin_auth1_token");
    content.append(QByteArray(4, '\0'));
    content.append("auth1-ascii");
    writeFile(levelDB, QStringLiteral("000006.ldb"), content);

    auto entries = ChromiumLocalStorageReader::readEntries(
        QStringLiteral("https://windsurf.com"), levelDB,
        {QStringLiteral("devin_session_token"), QStringLiteral("devin_auth1_token")});
    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries[0].key, QStringLiteral("devin_session_token"));
    QCOMPARE(entries[0].value, QStringLiteral("session-token-value-utf16"));
}

void tst_ChromiumLocalStorageReader::ignoresOtherOrigins()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());

    // File with only https://example.com
    QByteArray content;
    content.append("https://example.com");
    content.append(QByteArray(32, '\0'));
    content.append("devin_session_token");
    content.append(QByteArray(4, '\0'));
    content.append("fake-token");
    writeFile(levelDB, QStringLiteral("000003.ldb"), content);

    auto entries = ChromiumLocalStorageReader::readEntries(
        QStringLiteral("https://windsurf.com"), levelDB,
        {QStringLiteral("devin_session_token")});
    // Origin-aware extraction should only match windsurf.com
    QVERIFY(entries.isEmpty());
}

void tst_ChromiumLocalStorageReader::textFallbackExtraction()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());

    // File without any origin marker, just key/value as text
    QByteArray content("devin_auth1_token  auth1-value-xyz789");
    writeFile(levelDB, QStringLiteral("000001.log"), content);

    auto entries = ChromiumLocalStorageReader::readTextEntries(levelDB,
        {QStringLiteral("devin_auth1_token")});
    QVERIFY(!entries.isEmpty());
    QCOMPARE(entries[0].key, QStringLiteral("devin_auth1_token"));
    QVERIFY(entries[0].value.contains(QStringLiteral("auth1")));
}

void tst_ChromiumLocalStorageReader::readsNewestFileFirst()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());

    // Write older file first
    QByteArray oldContent("devin_session_token old-token-value");
    writeFile(levelDB, QStringLiteral("old.ldb"), oldContent);

    // Write newer file second (OS will give it a more recent timestamp)
    QByteArray newContent("devin_session_token new-token-value");
    writeFile(levelDB, QStringLiteral("new.ldb"), newContent);

    QFile oldFile(levelDB + QStringLiteral("/old.ldb"));
    QFile newFile(levelDB + QStringLiteral("/new.ldb"));

    QVERIFY(oldFile.open(QIODevice::ReadWrite));
    QVERIFY(newFile.open(QIODevice::ReadWrite));
    const QDateTime now = QDateTime::currentDateTimeUtc();
    QVERIFY(oldFile.setFileTime(now.addSecs(-60), QFileDevice::FileModificationTime));
    QVERIFY(newFile.setFileTime(now, QFileDevice::FileModificationTime));
    oldFile.close();
    newFile.close();

    QFileInfo oldInfo(levelDB + QStringLiteral("/old.ldb"));
    QFileInfo newInfo(levelDB + QStringLiteral("/new.ldb"));
    QVERIFY(newInfo.lastModified() >= oldInfo.lastModified());

    auto entries = ChromiumLocalStorageReader::readTextEntries(levelDB,
        {QStringLiteral("devin_session_token")});
    QVERIFY(!entries.isEmpty());
    QCOMPARE(entries[0].key, QStringLiteral("devin_session_token"));
    QCOMPARE(entries[0].value, QStringLiteral("new-token-value"));
}

void tst_ChromiumLocalStorageReader::ignoresNonLevelDBFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString levelDB = makeLevelDBDir(dir.path());

    // Write a .txt file (not .ldb or .log), should be ignored
    QByteArray content("devin_session_token txt-token");
    writeFile(levelDB, QStringLiteral("MANIFEST-000001"), content);
    writeFile(levelDB, QStringLiteral("CURRENT"), content);
    writeFile(levelDB, QStringLiteral("LOCK"), content);

    auto entries = ChromiumLocalStorageReader::readTextEntries(levelDB,
        {QStringLiteral("devin_session_token")});
    // Only .ldb and .log files are read
    QVERIFY(entries.isEmpty());
}

QTEST_MAIN(tst_ChromiumLocalStorageReader)
#include "tst_ChromiumLocalStorageReader.moc"
