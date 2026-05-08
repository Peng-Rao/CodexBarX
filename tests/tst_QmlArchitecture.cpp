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
    void costUsageViewDataIsLayered();
    void usageStoreLegacyApisAreNotQmlInvokable();
    void providerUiBuildersUseCatalogSnapshot();
    void costUsageScanUsesCostUsageService();
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

    QVERIFY2(contents.contains(QStringLiteral("providerStatuses")),
             "UsageStore must dispatch provider status polling through UsageBackend.");
    QVERIFY2(contents.contains(QStringLiteral("providerListModel")),
             "UsageStore must dispatch provider list preparation through UsageBackend.");
    QVERIFY2(contents.contains(QStringLiteral("providerDescriptorData")),
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
    QVERIFY2(storeContents.contains(QStringLiteral("QStringLiteral(\"costUsageDetailsRows\")")),
             "UsageStore must dispatch Usage details row preparation through UsageBackend.");
    QVERIFY2(storeContents.contains(QStringLiteral("CostUsageService::detailsRows")),
             "Cost usage details rows must be built by CostUsageService in the backend job.");

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

void QmlArchitectureTest::costUsageViewDataIsLayered()
{
    QFile usageStore(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(usageStore.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(usageStore.errorString()));
    const QString storeContents = QString::fromUtf8(usageStore.readAll());

    const QStringList requiredJobKinds = {
        QStringLiteral("QStringLiteral(\"costUsageSummary\")"),
        QStringLiteral("QStringLiteral(\"costUsageProviderRows\")"),
        QStringLiteral("QStringLiteral(\"costUsageDetailsRows\")"),
        QStringLiteral("QStringLiteral(\"costUsageProviderDetail\")"),
    };
    for (const QString& jobKind : requiredJobKinds) {
        QVERIFY2(storeContents.contains(jobKind),
                 qPrintable(QStringLiteral("Cost usage view data must be dispatched as layered backend jobs: %1").arg(jobKind)));
    }
    QVERIFY2(!storeContents.contains(QStringLiteral("QStringLiteral(\"costUsageViewData\")")),
             "UsageStore must not dispatch one combined costUsageViewData job.");

    const QString summaryStart = QStringLiteral("QVariantMap UsageStore::costUsageData() const");
    const QString providerRowsStart = QStringLiteral("QVariantList UsageStore::providerCostUsageList() const");
    const QString detailsRowsStart = QStringLiteral("QVariantList UsageStore::costUsageDetailsRows() const");
    const QString tokenCountStart = QStringLiteral("int UsageStore::costUsageTokenProviderCount() const");
    const int summaryStartIndex = storeContents.indexOf(summaryStart);
    QVERIFY2(summaryStartIndex >= 0, "Missing UsageStore::costUsageData().");
    const int providerRowsStartIndex = storeContents.indexOf(providerRowsStart, summaryStartIndex + summaryStart.size());
    QVERIFY2(providerRowsStartIndex > summaryStartIndex, "Missing UsageStore::providerCostUsageList().");
    const int detailsRowsStartIndex = storeContents.indexOf(detailsRowsStart, providerRowsStartIndex + providerRowsStart.size());
    QVERIFY2(detailsRowsStartIndex > providerRowsStartIndex, "Missing UsageStore::costUsageDetailsRows().");
    const int tokenCountStartIndex = storeContents.indexOf(tokenCountStart, detailsRowsStartIndex + detailsRowsStart.size());
    QVERIFY2(tokenCountStartIndex > detailsRowsStartIndex, "Missing UsageStore::costUsageTokenProviderCount().");

    const QString summaryBody = storeContents.mid(summaryStartIndex, providerRowsStartIndex - summaryStartIndex);
    QVERIFY2(summaryBody.contains(QStringLiteral("requestCostUsageSummary")),
             "costUsageData() must only schedule the summary builder.");
    QVERIFY2(!summaryBody.contains(QStringLiteral("requestCostUsageProviderRows")),
             "costUsageData() must not schedule provider row construction.");
    QVERIFY2(!summaryBody.contains(QStringLiteral("requestCostUsageDetailsRows")),
             "costUsageData() must not schedule Usage details rows.");

    const QString providerRowsBody = storeContents.mid(providerRowsStartIndex, detailsRowsStartIndex - providerRowsStartIndex);
    QVERIFY2(providerRowsBody.contains(QStringLiteral("requestCostUsageProviderRows")),
             "providerCostUsageList() must schedule only the provider rows builder.");
    QVERIFY2(!providerRowsBody.contains(QStringLiteral("requestCostUsageDetailsRows")),
             "providerCostUsageList() must not schedule Usage details rows.");

    const QString detailsRowsBody = storeContents.mid(detailsRowsStartIndex, tokenCountStartIndex - detailsRowsStartIndex);
    QVERIFY2(detailsRowsBody.contains(QStringLiteral("requestCostUsageDetailsRows")),
             "costUsageDetailsRows() must be the only getter that schedules Usage details rows.");
    QVERIFY2(!detailsRowsBody.contains(QStringLiteral("requestCostUsageSummary")),
             "costUsageDetailsRows() must not request a combined view-data build.");

    QFile trayViewModel(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/TrayViewModel.cpp"));
    QVERIFY2(trayViewModel.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(trayViewModel.errorString()));
    const QString trayContents = QString::fromUtf8(trayViewModel.readAll());
    const QString traySyncStart = QStringLiteral("void TrayViewModel::syncCostData()");
    const int traySyncStartIndex = trayContents.indexOf(traySyncStart);
    QVERIFY2(traySyncStartIndex >= 0, "Missing TrayViewModel::syncCostData().");
    const QString traySyncBody = trayContents.mid(traySyncStartIndex);
    QVERIFY2(!traySyncBody.contains(QStringLiteral("m_store->providerCostUsageList()")),
             "TrayViewModel must keep the tray-visible sync path to summary-only cost usage data.");

    QFile detailsViewModel(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageDetailsViewModel.cpp"));
    QVERIFY2(detailsViewModel.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(detailsViewModel.errorString()));
    const QString detailsVmContents = QString::fromUtf8(detailsViewModel.readAll());
    QVERIFY2(detailsVmContents.contains(QStringLiteral("requestCostUsageSummary")),
             "UsageDetailsViewModel must request the summary layer explicitly.");
    QVERIFY2(detailsVmContents.contains(QStringLiteral("requestCostUsageDetailsRows")),
             "UsageDetailsViewModel must request details rows only while active.");
    QVERIFY2(!detailsVmContents.contains(QStringLiteral("requestCostUsageViewData")),
             "UsageDetailsViewModel must not request a combined view-data build.");

    QFile service(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/CostUsageService.cpp"));
    QVERIFY2(service.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(service.errorString()));
    const QString serviceContents = QString::fromUtf8(service.readAll());
    QVERIFY2(serviceContents.contains(QStringLiteral("CostUsageService::summaryData")),
             "CostUsageService must own cost summary view-data construction.");
    QVERIFY2(serviceContents.contains(QStringLiteral("CostUsageService::providerRows")),
             "CostUsageService must own provider row view-data construction.");
    QVERIFY2(serviceContents.contains(QStringLiteral("CostUsageService::detailsRows")),
             "CostUsageService must own Usage details row view-data construction.");
}

void QmlArchitectureTest::usageStoreLegacyApisAreNotQmlInvokable()
{
    QFile usageStoreHeader(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.h"));
    QVERIFY2(usageStoreHeader.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(usageStoreHeader.errorString()));
    const QString headerContents = QString::fromUtf8(usageStoreHeader.readAll());

    const QStringList forbiddenInvokables = {
        QStringLiteral("Q_INVOKABLE void setProviderEnabled"),
        QStringLiteral("Q_INVOKABLE void setProviderSetting"),
        QStringLiteral("Q_INVOKABLE bool setProviderSecret"),
        QStringLiteral("Q_INVOKABLE bool clearProviderSecret"),
        QStringLiteral("Q_INVOKABLE void testProviderConnection"),
        QStringLiteral("Q_INVOKABLE void startProviderLogin"),
        QStringLiteral("Q_INVOKABLE void cancelProviderLogin"),
        QStringLiteral("Q_INVOKABLE void refreshProviderStatuses"),
        QStringLiteral("Q_INVOKABLE QVariantMap providerCostUsageData"),
        QStringLiteral("Q_INVOKABLE QVariantList providerSettingsFields"),
    };
    for (const QString& signature : forbiddenInvokables) {
        QVERIFY2(!headerContents.contains(signature),
                 qPrintable(QStringLiteral("Legacy UsageStore API must not remain QML-invokable: %1").arg(signature)));
    }

    QVERIFY2(!headerContents.contains(QStringLiteral("providerCostUsageData(")),
             "providerCostUsageData() must be removed after UsageDetailsModel/CostUsageService layering.");
    QVERIFY2(!headerContents.contains(QStringLiteral("providerSettingsFields(")),
             "providerSettingsFields() direct QML path must be removed; descriptor data carries settings fields.");

    QFile usageStoreSource(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(usageStoreSource.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(usageStoreSource.errorString()));
    const QString sourceContents = QString::fromUtf8(usageStoreSource.readAll());
    QVERIFY2(!sourceContents.contains(QStringLiteral("UsageStore::providerCostUsageData")),
             "providerCostUsageData() implementation must be deleted.");
    QVERIFY2(!sourceContents.contains(QStringLiteral("UsageStore::providerSettingsFields")),
             "providerSettingsFields() implementation must be deleted.");

    const QStringList qmlFiles = {
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/TrayPanel.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/SettingsWindow.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/UsageWindow.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/PlanUtilizationChart.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/panes/DebugPane.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/panes/ProvidersPane.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/panes/TokenUsagePane.qml"),
        QStringLiteral(PROJECT_SOURCE_DIR "/qml/components/ProviderDetailView.qml"),
    };
    const QStringList forbiddenQmlCalls = {
        QStringLiteral("UsageStore.setProviderEnabled("),
        QStringLiteral("UsageStore.setProviderSetting("),
        QStringLiteral("UsageStore.setProviderSecret("),
        QStringLiteral("UsageStore.clearProviderSecret("),
        QStringLiteral("UsageStore.testProviderConnection("),
        QStringLiteral("UsageStore.startProviderLogin("),
        QStringLiteral("UsageStore.cancelProviderLogin("),
        QStringLiteral("UsageStore.providerCostUsageData("),
        QStringLiteral("UsageStore.providerSettingsFields("),
    };
    for (const QString& fileName : qmlFiles) {
        QFile file(fileName);
        QVERIFY2(file.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(file.errorString()));
        const QString contents = QString::fromUtf8(file.readAll());
        for (const QString& call : forbiddenQmlCalls) {
            QVERIFY2(!contents.contains(call),
                     qPrintable(QStringLiteral("%1 must route legacy UsageStore access through a ViewModel/request command, not %2")
                                    .arg(fileName, call)));
        }
    }
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

    // Verify UsageStore delegates to ProviderUIService
    QVERIFY2(contents.contains(QStringLiteral("m_uiService->requestProviderList()")),
             "UsageStore must delegate provider list building to ProviderUIService.");
    QVERIFY2(contents.contains(QStringLiteral("m_uiService->requestProviderDescriptor")),
             "UsageStore must delegate provider descriptor building to ProviderUIService.");

    // Verify ProviderUIService uses catalog snapshot
    QFile uiService(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/ProviderUIService.cpp"));
    QVERIFY2(uiService.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(uiService.errorString()));
    const QString uiServiceContents = QString::fromUtf8(uiService.readAll());

    const QString listStart = QStringLiteral("void ProviderUIService::requestProviderList()");
    const QString listEnd = QStringLiteral("void ProviderUIService::requestProviderDescriptor");
    const int listStartIndex = uiServiceContents.indexOf(listStart);
    QVERIFY2(listStartIndex >= 0, "Missing ProviderUIService::requestProviderList().");
    const int listEndIndex = uiServiceContents.indexOf(listEnd, listStartIndex + listStart.size());
    QVERIFY2(listEndIndex > listStartIndex, "Missing method after ProviderUIService::requestProviderList().");
    const QString listBody = uiServiceContents.mid(listStartIndex, listEndIndex - listStartIndex);
    QVERIFY2(!listBody.contains(QStringLiteral("ProviderRegistry::instance()")),
             "Provider list backend input must read ProviderCatalogSnapshot, not live ProviderRegistry/provider QObject.");
    QVERIFY2(listBody.contains(QStringLiteral("m_catalog->providers()")),
             "Provider list backend input must iterate catalog snapshot entries.");

    const QString descriptorStart = QStringLiteral("void ProviderUIService::requestProviderDescriptor");
    const QString descriptorEnd = QStringLiteral("void ProviderUIService::invalidateProviderListCache");
    const int descriptorStartIndex = uiServiceContents.indexOf(descriptorStart);
    QVERIFY2(descriptorStartIndex >= 0, "Missing ProviderUIService::requestProviderDescriptor().");
    const int descriptorEndIndex = uiServiceContents.indexOf(descriptorEnd, descriptorStartIndex + descriptorStart.size());
    QVERIFY2(descriptorEndIndex > descriptorStartIndex, "Missing method after ProviderUIService::requestProviderDescriptor().");
    const QString descriptorBody = uiServiceContents.mid(descriptorStartIndex, descriptorEndIndex - descriptorStartIndex);
    QVERIFY2(!descriptorBody.contains(QStringLiteral("ProviderRegistry::instance()")),
             "Provider descriptor backend input must read ProviderCatalogSnapshot, not live ProviderRegistry/provider QObject.");
    QVERIFY2(!descriptorBody.contains(QStringLiteral("settingsDescriptors()")),
             "Provider descriptor backend input must use snapshotted settings descriptors.");
    QVERIFY2(descriptorBody.contains(QStringLiteral("m_catalog->provider(providerId)")),
             "Provider descriptor backend input must look up provider metadata in the catalog snapshot.");

    QVERIFY2(!contents.contains(QStringLiteral("UsageStore::providerSettingsFields")),
             "Provider settings fields must be prepared through providerDescriptorData instead of a direct QML getter.");

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

void QmlArchitectureTest::costUsageScanUsesCostUsageService()
{
    QFile serviceHeader(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/CostUsageService.h"));
    QVERIFY2(serviceHeader.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(serviceHeader.errorString()));
    const QString serviceHeaderContents = QString::fromUtf8(serviceHeader.readAll());
    QVERIFY2(serviceHeaderContents.contains(QStringLiteral("CostUsageScanPlan")),
             "CostUsageService must own the cost scan plan DTO.");

    QFile serviceSource(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/CostUsageService.cpp"));
    QVERIFY2(serviceSource.open(QIODevice::ReadOnly | QIODevice::Text),
             qPrintable(serviceSource.errorString()));
    const QString serviceContents = QString::fromUtf8(serviceSource.readAll());
    QVERIFY2(serviceContents.contains(QStringLiteral("CostUsageScanner scanner")),
             "CostUsageService must own CostUsageScanner work.");
    QVERIFY2(serviceContents.contains(QStringLiteral("CostUsageCache::instance()")),
             "CostUsageService must own CostUsageCache loading/saving.");

    QFile usageStore(QStringLiteral(PROJECT_SOURCE_DIR "/src/app/UsageStore.cpp"));
    QVERIFY2(usageStore.open(QIODevice::ReadOnly | QIODevice::Text), qPrintable(usageStore.errorString()));
    const QString contents = QString::fromUtf8(usageStore.readAll());
    QVERIFY2(!contents.contains(QStringLiteral("#include \"../util/CostUsageScanner.h\"")),
             "UsageStore must not include CostUsageScanner directly after CostUsageService extraction.");
    QVERIFY2(!contents.contains(QStringLiteral("#include \"../util/CostUsageCache.h\"")),
             "UsageStore must not include CostUsageCache directly after CostUsageService extraction.");

    const QString refreshStart = QStringLiteral("void UsageStore::refreshCostUsage()");
    const QString refreshEnd = QStringLiteral("QVariantMap UsageStore::costUsageData()");
    const int refreshStartIndex = contents.indexOf(refreshStart);
    QVERIFY2(refreshStartIndex >= 0, "Missing UsageStore::refreshCostUsage().");
    const int refreshEndIndex = contents.indexOf(refreshEnd, refreshStartIndex + refreshStart.size());
    QVERIFY2(refreshEndIndex > refreshStartIndex, "Missing method after UsageStore::refreshCostUsage().");
    const QString refreshBody = contents.mid(refreshStartIndex, refreshEndIndex - refreshStartIndex);
    QVERIFY2(refreshBody.contains(QStringLiteral("CostUsageService::buildScanPlan")),
             "UsageStore must build cost scan plans through CostUsageService.");
    QVERIFY2(refreshBody.contains(QStringLiteral("costUsageSubscribedProviderIDs")),
             "UsageStore must pass provider subscriptions into the cost scan plan.");
    QVERIFY2(refreshBody.contains(QStringLiteral("CostUsageService::refresh")),
             "UsageStore must dispatch CostUsageService refresh work.");
    QVERIFY2(!refreshBody.contains(QStringLiteral("CostUsageScanner")),
             "UsageStore refresh path must not instantiate or reference CostUsageScanner.");
    QVERIFY2(!refreshBody.contains(QStringLiteral("CostUsageCache")),
             "UsageStore refresh path must not load/save CostUsageCache.");
    QVERIFY2(!refreshBody.contains(QStringLiteral("scanOpenCodeDB")),
             "UsageStore refresh path must not own SQLite/opencode scan details.");

    QVERIFY2(serviceContents.contains(QStringLiteral("scanOpenCodeDB(since, today, plan.openCodeDBProviderIds)")),
             "CostUsageService must pass an enabled+subscribed provider allow-list into OpenCode DB scans.");
}

QTEST_MAIN(QmlArchitectureTest)

#include "tst_QmlArchitecture.moc"
