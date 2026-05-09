#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "providers/codex/CodexAtomicFileSwap.h"
#include "providers/codex/CodexPromotionError.h"

class tst_CodexAtomicFileSwap : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // CodexAtomicFileSwap tests
    void test_stageFile_createsTempFile();
    void test_stageFile_writesCorrectData();
    void test_commit_movesToTarget();
    void test_commit_failsWithoutStage();
    void test_rollback_removesTempFile();
    void test_destructor_removesUncommittedFile();
    void test_commit_createsTargetDirectory();

    // CodexPromotionError tests
    void test_errorMapper_returnsCorrectTitle();
    void test_errorMapper_returnsCorrectMessage();
    void test_errorMapper_noneIsEmpty();

private:
    QTemporaryDir m_tempDir;
};

void tst_CodexAtomicFileSwap::initTestCase()
{
    QVERIFY(m_tempDir.isValid());
}

void tst_CodexAtomicFileSwap::cleanupTestCase()
{
}

// --- CodexAtomicFileSwap Tests ---

void tst_CodexAtomicFileSwap::test_stageFile_createsTempFile()
{
    QString targetPath = m_tempDir.filePath("test1.txt");
    CodexAtomicFileSwap swapper(targetPath);

    QVERIFY(swapper.stageFile(QByteArray("test data")));
    QVERIFY(swapper.hasStagedFile());

    // Staged file should exist with correct prefix
    QStringList matches = QDir(m_tempDir.path()).entryList(QStringList() << "test1.txt.codexbarx-staged*", QDir::Files);
    QCOMPARE(matches.size(), 1);
}

void tst_CodexAtomicFileSwap::test_stageFile_writesCorrectData()
{
    QString targetPath = m_tempDir.filePath("test2.txt");
    CodexAtomicFileSwap swapper(targetPath);

    QByteArray testData = "Hello, World!";
    QVERIFY(swapper.stageFile(testData));

    // Find the staged file and verify contents
    QStringList matches = QDir(m_tempDir.path()).entryList(QStringList() << "test2.txt.codexbarx-staged*", QDir::Files);
    QCOMPARE(matches.size(), 1);

    QFile stagedFile(m_tempDir.filePath(matches.first()));
    QVERIFY(stagedFile.open(QIODevice::ReadOnly));
    QCOMPARE(stagedFile.readAll(), testData);
    stagedFile.close();
}

void tst_CodexAtomicFileSwap::test_commit_movesToTarget()
{
    QString targetPath = m_tempDir.filePath("test3.txt");
    CodexAtomicFileSwap swapper(targetPath);

    QByteArray testData = "Commit test data";
    QVERIFY(swapper.stageFile(testData));
    QVERIFY(swapper.commit());

    // Target file should now exist with correct content
    QFile targetFile(targetPath);
    QVERIFY(targetFile.exists());
    QVERIFY(targetFile.open(QIODevice::ReadOnly));
    QCOMPARE(targetFile.readAll(), testData);
    targetFile.close();

    // Staged file should be gone
    QVERIFY(!swapper.hasStagedFile());
}

void tst_CodexAtomicFileSwap::test_commit_failsWithoutStage()
{
    QString targetPath = m_tempDir.filePath("test4.txt");
    CodexAtomicFileSwap swapper(targetPath);

    QVERIFY(!swapper.commit());
    QVERIFY(!swapper.errorMessage().isEmpty());
}

void tst_CodexAtomicFileSwap::test_rollback_removesTempFile()
{
    QString targetPath = m_tempDir.filePath("test5.txt");
    CodexAtomicFileSwap swapper(targetPath);

    QVERIFY(swapper.stageFile(QByteArray("rollback test")));
    QVERIFY(swapper.hasStagedFile());

    swapper.rollback();

    QVERIFY(!swapper.hasStagedFile());

    // Staged file should be removed
    QStringList matches = QDir(m_tempDir.path()).entryList(QStringList() << "test5.txt.codexbarx-staged*", QDir::Files);
    QCOMPARE(matches.size(), 0);
}

void tst_CodexAtomicFileSwap::test_destructor_removesUncommittedFile()
{
    QString targetPath = m_tempDir.filePath("test6.txt");

    {
        CodexAtomicFileSwap swapper(targetPath);
        QVERIFY(swapper.stageFile(QByteArray("destructor test")));
        // Don't commit - destructor should clean up
    }

    // Staged file should be removed by destructor
    QStringList matches = QDir(m_tempDir.path()).entryList(QStringList() << "test6.txt.codexbarx-staged*", QDir::Files);
    QCOMPARE(matches.size(), 0);
}

void tst_CodexAtomicFileSwap::test_commit_createsTargetDirectory()
{
    // The commit() method should create parent directories if needed
    QString subDir = m_tempDir.filePath("subdir/nested");
    QString targetPath = subDir + "/test7.txt";

    // Create the subdirectory first (stageFile needs existing directory)
    QVERIFY(QDir().mkpath(subDir));

    CodexAtomicFileSwap swapper(targetPath);

    QByteArray testData = "Nested directory test";
    QVERIFY(swapper.stageFile(testData));
    QVERIFY(swapper.commit());

    // Target file should exist with correct content
    QFile targetFile(targetPath);
    QVERIFY(targetFile.exists());
    QVERIFY(targetFile.open(QIODevice::ReadOnly));
    QCOMPARE(targetFile.readAll(), testData);
}

// --- CodexPromotionError Tests ---

void tst_CodexAtomicFileSwap::test_errorMapper_returnsCorrectTitle()
{
    QString title = CodexPromotionErrorMapper::titleForError(CodexPromotionError::TargetManagedAccountNotFound);
    QVERIFY(!title.isEmpty());
    QVERIFY(title.contains("switch system account", Qt::CaseInsensitive));

    QString titleInProgress = CodexPromotionErrorMapper::titleForError(CodexPromotionError::AlreadyInProgress);
    QVERIFY(titleInProgress.contains("progress", Qt::CaseInsensitive));
}

void tst_CodexAtomicFileSwap::test_errorMapper_returnsCorrectMessage()
{
    QString message = CodexPromotionErrorMapper::messageForError(CodexPromotionError::TargetManagedAccountNotFound);
    QVERIFY(!message.isEmpty());
    QVERIFY(message.contains("CodexBarX", Qt::CaseInsensitive));

    QString messageApi = CodexPromotionErrorMapper::messageForError(CodexPromotionError::LiveAccountAPIKeyOnlyUnsupported);
    QVERIFY(messageApi.contains("API key", Qt::CaseInsensitive));
}

void tst_CodexAtomicFileSwap::test_errorMapper_noneIsEmpty()
{
    QString title = CodexPromotionErrorMapper::titleForError(CodexPromotionError::None);
    QString message = CodexPromotionErrorMapper::messageForError(CodexPromotionError::None);

    QVERIFY(title.isEmpty());
    QVERIFY(message.isEmpty());

    CodexSystemAccountPromotionUserFacingError error = CodexPromotionErrorMapper::map(CodexPromotionError::None);
    QVERIFY(error.isEmpty());
}

QTEST_MAIN(tst_CodexAtomicFileSwap)
#include "tst_CodexAtomicFileSwap.moc"
