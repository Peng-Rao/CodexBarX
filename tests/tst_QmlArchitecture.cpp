#include <QtTest/QtTest>

#include <QFile>
#include <QStringList>

class QmlArchitectureTest : public QObject {
    Q_OBJECT

private slots:
    void trayPanelDoesNotUseSynchronousUsageStoreGetters();
    void tokenUsagePaneDoesNotUseSynchronousUsageStoreGetters();
    void usageStoreInteractiveProviderJobsUseBackend();
    void usageStoreSettingsAndStatusJobsUseBackend();
    void usageStoreCleanupJobsUseBackend();
};

void QmlArchitectureTest::trayPanelDoesNotUseSynchronousUsageStoreGetters()
{
    QFile file(QStringLiteral(PROJECT_SOURCE_DIR "/qml/TrayPanel.qml"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    const QString contents = QString::fromUtf8(file.readAll());
    const QStringList forbiddenCalls = {
        QStringLiteral("UsageStore.snapshotData("),
        QStringLiteral("UsageStore.tokenAccountsForProvider("),
        QStringLiteral("UsageStore.defaultTokenAccount("),
        QStringLiteral("UsageStore.providerStatusURL("),
        QStringLiteral("UsageStore.providerDashboardData("),
        QStringLiteral("UsageStore.providerCostUsageList("),
        QStringLiteral("UsageStore.costUsageData("),
    };

    for (const QString& call : forbiddenCalls) {
        QVERIFY2(!contents.contains(call),
                 qPrintable(QStringLiteral("TrayPanel.qml must read cached view-model state instead of calling %1").arg(call)));
    }
}

void QmlArchitectureTest::tokenUsagePaneDoesNotUseSynchronousUsageStoreGetters()
{
    QFile file(QStringLiteral(PROJECT_SOURCE_DIR "/qml/panes/TokenUsagePane.qml"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    const QString contents = QString::fromUtf8(file.readAll());
    const QStringList forbiddenCalls = {
        QStringLiteral("UsageStore.costUsageData("),
        QStringLiteral("UsageStore.providerCostUsageList("),
        QStringLiteral("UsageStore.providerList("),
    };

    for (const QString& call : forbiddenCalls) {
        QVERIFY2(!contents.contains(call),
                 qPrintable(QStringLiteral("TokenUsagePane.qml must read cached view-model state instead of calling %1").arg(call)));
    }
}

void QmlArchitectureTest::usageStoreInteractiveProviderJobsUseBackend()
{
    QFile file(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    const QString contents = QString::fromUtf8(file.readAll());
    const QStringList forbiddenSnippets = {
        QStringLiteral("QtConcurrent::run(pool ? pool : m_threadPool"),
        QStringLiteral("QtConcurrent::run(m_interactiveThreadPool"),
    };

    for (const QString& snippet : forbiddenSnippets) {
        QVERIFY2(!contents.contains(snippet),
                 qPrintable(QStringLiteral("Interactive provider jobs must dispatch through UsageBackend, not %1").arg(snippet)));
    }

    QVERIFY2(contents.contains(QStringLiteral("QStringLiteral(\"providerRefresh\")")),
             "UsageStore must dispatch provider refresh through UsageBackend.");
    QVERIFY2(contents.contains(QStringLiteral("QStringLiteral(\"providerConnectionTest\")")),
             "UsageStore must dispatch Test Connection through UsageBackend.");
}

void QmlArchitectureTest::usageStoreSettingsAndStatusJobsUseBackend()
{
    QFile file(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    const QString contents = QString::fromUtf8(file.readAll());
    const QStringList forbiddenSnippets = {
        QStringLiteral("buildProviderListNow();\n        m_providerListRefreshQueued = false"),
        QStringLiteral("buildProviderDescriptorDataNow(providerId);\n        m_providerDescriptorRefreshQueued.remove"),
        QStringLiteral("QtConcurrent::run(m_threadPool, [batch, finishBatch, target]"),
        QStringLiteral("QtConcurrent::run(m_threadPool, [batch, finishBatch, workspaceURL"),
    };

    for (const QString& snippet : forbiddenSnippets) {
        QVERIFY2(!contents.contains(snippet),
                 qPrintable(QStringLiteral("Settings/status preparation must dispatch through UsageBackend, not %1").arg(snippet)));
    }

    QVERIFY2(contents.contains(QStringLiteral("QStringLiteral(\"providerStatuses\")")),
             "UsageStore must dispatch provider status polling through UsageBackend.");
    QVERIFY2(contents.contains(QStringLiteral("QStringLiteral(\"providerListModel\")")),
             "UsageStore must dispatch provider list preparation through UsageBackend.");
    QVERIFY2(contents.contains(QStringLiteral("QStringLiteral(\"providerDescriptorData\")")),
             "UsageStore must dispatch provider descriptor preparation through UsageBackend.");
}

void QmlArchitectureTest::usageStoreCleanupJobsUseBackend()
{
    QFile file(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));

    const QString contents = QString::fromUtf8(file.readAll());
    QVERIFY2(!contents.contains(QStringLiteral("QtConcurrent::run(m_threadPool")),
             "UsageStore cleanup jobs must dispatch through UsageBackend instead of owning thread-pool jobs.");

    const QStringList requiredJobKinds = {
        QStringLiteral("QStringLiteral(\"codexCreditsRefresh\")"),
        QStringLiteral("QStringLiteral(\"credentialStatusCheck\")"),
        QStringLiteral("QStringLiteral(\"providerSecretWrite\")"),
        QStringLiteral("QStringLiteral(\"providerSecretRemove\")"),
        QStringLiteral("QStringLiteral(\"providerLoginStart\")"),
        QStringLiteral("QStringLiteral(\"providerLoginPoll\")"),
    };

    for (const QString& jobKind : requiredJobKinds) {
        QVERIFY2(contents.contains(jobKind),
                 qPrintable(QStringLiteral("UsageStore must dispatch cleanup job through UsageBackend: %1").arg(jobKind)));
    }
}

QTEST_MAIN(QmlArchitectureTest)

#include "tst_QmlArchitecture.moc"
