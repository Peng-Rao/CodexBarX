#include <QtTest/QtTest>

#include <QFile>
#include <QStringList>

class QmlArchitectureTest : public QObject {
    Q_OBJECT

private slots:
    void trayPanelDoesNotUseSynchronousUsageStoreGetters();
    void tokenUsagePaneDoesNotUseSynchronousUsageStoreGetters();
    void usageStoreInteractiveProviderJobsUseBackend();
    void usageStoreBackendJobsDoNotCaptureStore();
    void usageStoreSettingsAndStatusJobsUseBackend();
    void usageStoreCleanupJobsUseBackend();
    void settingsProvidersUsesSettingsProvidersModel();
    void usageDetailsRowsArePreparedByBackend();
    void providerUiBuildersUseCatalogSnapshot();
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

void QmlArchitectureTest::usageStoreBackendJobsDoNotCaptureStore()
{
    QFile usageStore(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(usageStore.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(usageStore.errorString()));
    const QString contents = QString::fromUtf8(usageStore.readAll());

    const QStringList forbiddenSnippets = {
        QStringLiteral("QStringLiteral(\"providerRefresh\"), 0, [this"),
        QStringLiteral("QStringLiteral(\"providerConnectionTest\"), 0,\n        [this"),
        QStringLiteral("QtConcurrent::run(usageStore->threadPool()"),
        QStringLiteral("m_threadPool"),
        QStringLiteral("m_interactiveThreadPool"),
    };

    for (const QString& snippet : forbiddenSnippets) {
        QVERIFY2(!contents.contains(snippet),
                 qPrintable(QStringLiteral("UsageStore must only dispatch/apply backend work, not retain old worker ownership: %1").arg(snippet)));
    }

    QVERIFY2(contents.contains(QStringLiteral("UsageBackendJobs::refreshProvider")),
             "Provider refresh worker logic must live behind UsageBackendJobs.");
    QVERIFY2(contents.contains(QStringLiteral("UsageBackendJobs::testProviderConnection")),
             "Connection test worker logic must live behind UsageBackendJobs.");
    QVERIFY2(contents.contains(QStringLiteral("UsageBackendJobs::preloadCredentials")),
             "Credential preload worker logic must live behind UsageBackendJobs.");

    QFile mainFile(QStringLiteral(PROJECT_SOURCE_DIR "/src/main.cpp"));
    QVERIFY2(mainFile.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(mainFile.errorString()));
    const QString mainContents = QString::fromUtf8(mainFile.readAll());
    QVERIFY2(!mainContents.contains(QStringLiteral("QtConcurrent::run(usageStore->threadPool()")),
             "main.cpp must request credential preload through UsageStore/UsageBackend instead of owning a thread-pool job.");
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

void QmlArchitectureTest::settingsProvidersUsesSettingsProvidersModel()
{
    QFile settingsWindow(QStringLiteral(PROJECT_SOURCE_DIR "/qml/SettingsWindow.qml"));
    QVERIFY2(settingsWindow.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(settingsWindow.errorString()));
    const QString settingsContents = QString::fromUtf8(settingsWindow.readAll());

    QVERIFY2(settingsContents.contains(QStringLiteral("SettingsProvidersModel")),
             "SettingsWindow.qml must route Providers tab state through SettingsProvidersModel.");

    const QStringList settingsForbiddenCalls = {
        QStringLiteral("UsageStore.providerList("),
        QStringLiteral("UsageStore.providerDescriptorData("),
        QStringLiteral("UsageStore.requestProviderList("),
        QStringLiteral("UsageStore.requestProviderDescriptor("),
    };
    for (const QString& call : settingsForbiddenCalls) {
        QVERIFY2(!settingsContents.contains(call),
                 qPrintable(QStringLiteral("SettingsWindow.qml must not call legacy UsageStore provider APIs: %1").arg(call)));
    }

    QFile providersPane(QStringLiteral(PROJECT_SOURCE_DIR "/qml/panes/ProvidersPane.qml"));
    QVERIFY2(providersPane.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(providersPane.errorString()));
    const QString providersContents = QString::fromUtf8(providersPane.readAll());
    QVERIFY2(providersContents.contains(QStringLiteral("model.providerId")),
             "ProvidersPane.qml must consume QAbstractListModel roles for provider rows.");
    QVERIFY2(!providersContents.contains(QStringLiteral("UsageStore.moveProvider(")),
             "ProvidersPane.qml must emit a command signal instead of calling UsageStore.moveProvider().");

    QFile providerDetail(QStringLiteral(PROJECT_SOURCE_DIR "/qml/components/ProviderDetailView.qml"));
    QVERIFY2(providerDetail.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(providerDetail.errorString()));
    const QString detailContents = QString::fromUtf8(providerDetail.readAll());

    const QStringList detailForbiddenCalls = {
        QStringLiteral("UsageStore.tokenAccountsForProvider("),
        QStringLiteral("UsageStore.defaultTokenAccount("),
        QStringLiteral("UsageStore.requestAddTokenAccount("),
        QStringLiteral("UsageStore.requestAddTokenAccountWithApiKey("),
        QStringLiteral("UsageStore.requestRemoveTokenAccount("),
        QStringLiteral("UsageStore.requestSetDefaultTokenAccount("),
        QStringLiteral("UsageStore.requestSetTokenAccountSourceMode("),
        QStringLiteral("UsageStore.requestSetTokenAccountVisibility("),
        QStringLiteral("UsageStore.codexAccountState"),
        QStringLiteral("UsageStore.codexConsumerProjectionData("),
    };

    for (const QString& call : detailForbiddenCalls) {
        QVERIFY2(!detailContents.contains(call),
                 qPrintable(QStringLiteral("ProviderDetailView.qml must bind prepared SettingsProvidersModel state instead of %1").arg(call)));
    }
}

void QmlArchitectureTest::usageDetailsRowsArePreparedByBackend()
{
    QFile viewModel(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageDetailsViewModel.cpp"));
    QVERIFY2(viewModel.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(viewModel.errorString()));
    const QString vmContents = QString::fromUtf8(viewModel.readAll());

    const QStringList forbiddenVmSnippets = {
        QStringLiteral("buildProviderRows("),
        QStringLiteral("m_store->providerList("),
        QStringLiteral("m_store->providerCostUsageList("),
    };
    for (const QString& snippet : forbiddenVmSnippets) {
        QVERIFY2(!vmContents.contains(snippet),
                 qPrintable(QStringLiteral("UsageDetailsViewModel must consume backend-prepared rows, not %1").arg(snippet)));
    }
    QVERIFY2(vmContents.contains(QStringLiteral("m_store->costUsageDetailsRows()")),
             "UsageDetailsViewModel must read backend-prepared details rows.");
    QVERIFY2(vmContents.contains(QStringLiteral("requestCostUsageProviderDetail")),
             "UsageDetailsViewModel must request provider detail lazily.");

    QFile usageStore(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(usageStore.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(usageStore.errorString()));
    const QString storeContents = QString::fromUtf8(usageStore.readAll());
    QVERIFY2(storeContents.contains(QStringLiteral("QStringLiteral(\"costUsageProviderDetail\")")),
             "UsageStore must dispatch provider detail preparation through UsageBackend.");
    QVERIFY2(storeContents.contains(QStringLiteral("usageDetailsRows(allProviders")),
             "Cost usage view data must build Usage details rows in the backend job.");

    QFile pane(QStringLiteral(PROJECT_SOURCE_DIR "/qml/panes/TokenUsagePane.qml"));
    QVERIFY2(pane.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(pane.errorString()));
    const QString paneContents = QString::fromUtf8(pane.readAll());
    QVERIFY2(paneContents.contains(QStringLiteral("UsageDetailsViewModel.requestProviderDetail")),
             "TokenUsagePane must request model breakdown only when a provider is expanded.");
    QVERIFY2(paneContents.contains(QStringLiteral("property bool expanded: false")),
             "Provider usage cards must start collapsed.");
    QVERIFY2(!paneContents.contains(QStringLiteral("model: card.provider.models")),
             "TokenUsagePane must not render model breakdown from first-screen provider rows.");
}

void QmlArchitectureTest::providerUiBuildersUseCatalogSnapshot()
{
    QFile catalogHeader(QStringLiteral(PROJECT_SOURCE_DIR "/src/providers/ProviderCatalogSnapshot.h"));
    QVERIFY2(catalogHeader.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(catalogHeader.errorString()));
    const QString catalogContents = QString::fromUtf8(catalogHeader.readAll());
    QVERIFY2(catalogContents.contains(QStringLiteral("ProviderCatalogEntry")),
             "Provider catalog snapshot must expose immutable provider entries.");

    QFile usageStore(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(usageStore.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(usageStore.errorString()));
    const QString contents = QString::fromUtf8(usageStore.readAll());
    QVERIFY2(contents.contains(QStringLiteral("ProviderCatalogSnapshot::fromRegistry")),
             "UsageStore must rebuild a ProviderCatalogSnapshot from the registry at state boundaries.");

    const QString listStart = QStringLiteral("void UsageStore::requestProviderList()");
    const QString listEnd = QStringLiteral("void UsageStore::moveProvider");
    const int listStartIndex = contents.indexOf(listStart);
    QVERIFY2(listStartIndex >= 0, "Missing UsageStore::requestProviderList().");
    const int listEndIndex = contents.indexOf(listEnd, listStartIndex + listStart.size());
    QVERIFY2(listEndIndex > listStartIndex, "Missing method after UsageStore::requestProviderList().");
    const QString listBody = contents.mid(listStartIndex, listEndIndex - listStartIndex);
    QVERIFY2(!listBody.contains(QStringLiteral("ProviderRegistry::instance()")),
             "Provider list backend input must read ProviderCatalogSnapshot, not live ProviderRegistry/provider QObject.");
    QVERIFY2(listBody.contains(QStringLiteral("const ProviderCatalogSnapshot catalog = m_providerCatalog")),
             "Provider list backend input must copy the current catalog snapshot before dispatch.");
    QVERIFY2(listBody.contains(QStringLiteral("catalog.providers()")),
             "Provider list backend input must iterate catalog snapshot entries.");

    const QString descriptorStart = QStringLiteral("void UsageStore::requestProviderDescriptor");
    const QString descriptorEnd = QStringLiteral("QVariantList UsageStore::providerSettingsFields");
    const int descriptorStartIndex = contents.indexOf(descriptorStart);
    QVERIFY2(descriptorStartIndex >= 0, "Missing UsageStore::requestProviderDescriptor().");
    const int descriptorEndIndex = contents.indexOf(descriptorEnd, descriptorStartIndex + descriptorStart.size());
    QVERIFY2(descriptorEndIndex > descriptorStartIndex, "Missing method after UsageStore::requestProviderDescriptor().");
    const QString descriptorBody = contents.mid(descriptorStartIndex, descriptorEndIndex - descriptorStartIndex);
    QVERIFY2(!descriptorBody.contains(QStringLiteral("ProviderRegistry::instance()")),
             "Provider descriptor backend input must read ProviderCatalogSnapshot, not live ProviderRegistry/provider QObject.");
    QVERIFY2(!descriptorBody.contains(QStringLiteral("settingsDescriptors()")),
             "Provider descriptor backend input must use snapshotted settings descriptors.");
    QVERIFY2(descriptorBody.contains(QStringLiteral("m_providerCatalog.provider(providerId)")),
             "Provider descriptor backend input must look up provider metadata in the catalog snapshot.");

    const QString settingsStart = QStringLiteral("QVariantList UsageStore::providerSettingsFields");
    const QString settingsEnd = QStringLiteral("void UsageStore::setProviderSetting");
    const int settingsStartIndex = contents.indexOf(settingsStart);
    QVERIFY2(settingsStartIndex >= 0, "Missing UsageStore::providerSettingsFields().");
    const int settingsEndIndex = contents.indexOf(settingsEnd, settingsStartIndex + settingsStart.size());
    QVERIFY2(settingsEndIndex > settingsStartIndex, "Missing method after UsageStore::providerSettingsFields().");
    const QString settingsBody = contents.mid(settingsStartIndex, settingsEndIndex - settingsStartIndex);
    QVERIFY2(!settingsBody.contains(QStringLiteral("ProviderRegistry::instance()")),
             "Provider settings field builder must use snapshotted setting descriptors.");

    QFile bootstrap(QStringLiteral(PROJECT_SOURCE_DIR "/src/providers/ProviderBootstrap.cpp"));
    QVERIFY2(bootstrap.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(bootstrap.errorString()));
    const QString bootstrapContents = QString::fromUtf8(bootstrap.readAll());
    QVERIFY2(bootstrapContents.contains(QStringLiteral("ProviderCatalogSnapshot::fromRegistry")),
             "Provider bootstrap must use the catalog snapshot for provider default/enabled metadata.");
    QVERIFY2(!bootstrapContents.contains(QStringLiteral("provider->defaultEnabled()")),
             "Provider bootstrap must not query live provider metadata after the catalog snapshot is available.");

    QFile cliUsage(QStringLiteral(PROJECT_SOURCE_DIR "/src/cli/CLIUsageCommand.cpp"));
    QVERIFY2(cliUsage.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(cliUsage.errorString()));
    const QString cliUsageContents = QString::fromUtf8(cliUsage.readAll());
    QVERIFY2(cliUsageContents.contains(QStringLiteral("ProviderCatalogSnapshot::fromRegistry")),
             "CLI usage command must share provider metadata through ProviderCatalogSnapshot.");
    QVERIFY2(!cliUsageContents.contains(QStringLiteral("allProviders()")),
             "CLI usage command must use catalog enabled IDs instead of iterating live provider objects for metadata.");
}

QTEST_MAIN(QmlArchitectureTest)

#include "tst_QmlArchitecture.moc"
