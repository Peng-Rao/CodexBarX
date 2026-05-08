#include "app/UsageBackend.h"
#include "app/UsageBackendTypes.h"

#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QThread>

class UsageBackendTest : public QObject {
    Q_OBJECT

private slots:
    void valueJobRunsOffCallerThreadAndReturnsRequestMetadata();
};

void UsageBackendTest::valueJobRunsOffCallerThreadAndReturnsRequestMetadata()
{
    UsageBackend backend;
    QSignalSpy spy(&backend, &UsageBackend::jobFinished);
    QVERIFY(spy.isValid());

    const quintptr callerThread = reinterpret_cast<quintptr>(QThread::currentThreadId());
    const UsageBackendRequest request = backend.dispatchValueJob(
        QStringLiteral("thread-check"),
        42,
        [callerThread]() -> QVariant {
            QVariantMap payload;
            payload.insert(QStringLiteral("callerThread"), QString::number(callerThread));
            payload.insert(QStringLiteral("workerThread"),
                           QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId())));
            return payload;
        });

    QVERIFY(!request.requestId.isEmpty());
    QCOMPARE(request.kind, QStringLiteral("thread-check"));
    QCOMPARE(request.generation, 42);

    QTRY_COMPARE(spy.count(), 1);
    const auto result = qvariant_cast<UsageBackendResult>(spy.takeFirst().at(0));
    QCOMPARE(result.requestId, request.requestId);
    QCOMPARE(result.kind, request.kind);
    QCOMPARE(result.generation, 42);
    QVERIFY(result.success);

    const QVariantMap payload = result.payload.toMap();
    QVERIFY(!payload.value(QStringLiteral("workerThread")).toString().isEmpty());
    QCOMPARE(payload.value(QStringLiteral("callerThread")).toString(), QString::number(callerThread));
    QVERIFY2(payload.value(QStringLiteral("workerThread")).toString() != QString::number(callerThread),
             "UsageBackend jobs must execute off the caller/UI thread");
}

QTEST_MAIN(UsageBackendTest)

#include "tst_UsageBackend.moc"
