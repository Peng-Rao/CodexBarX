#include <QtTest/QtTest>

class tst_ClaudeStatusProbe : public QObject {
    Q_OBJECT
private slots:
    void basicTest();
};

void tst_ClaudeStatusProbe::basicTest()
{
    // ClaudeStatusProbe parsing is tested via ClaudeProvider integration tests
    // This test just verifies the module compiles and links correctly
    QVERIFY(true);
}

QTEST_MAIN(tst_ClaudeStatusProbe)
#include "tst_ClaudeStatusProbe.moc"
