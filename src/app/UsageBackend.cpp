#include "UsageBackend.h"

#include <QPointer>
#include <QUuid>

class UsageBackendWorker : public QObject {
    Q_OBJECT

public:
    using ValueJob = UsageBackend::ValueJob;

    void runValueJob(const UsageBackendRequest& request, ValueJob job)
    {
        UsageBackendResult result;
        result.requestId = request.requestId;
        result.kind = request.kind;
        result.generation = request.generation;

        try {
            result.payload = job ? job() : QVariant();
            result.success = true;
        } catch (const std::exception& ex) {
            result.success = false;
            result.message = QString::fromUtf8(ex.what());
        } catch (...) {
            result.success = false;
            result.message = QStringLiteral("Unknown backend job failure.");
        }

        emit valueJobFinished(result);
    }

signals:
    void valueJobFinished(const UsageBackendResult& result);
};

UsageBackend::UsageBackend(QObject* parent)
    : QObject(parent)
    , m_worker(new UsageBackendWorker)
{
    qRegisterMetaType<UsageBackendRequest>("UsageBackendRequest");
    qRegisterMetaType<UsageBackendResult>("UsageBackendResult");
    qRegisterMetaType<CostUsageSummaryPayload>("CostUsageSummaryPayload");
    qRegisterMetaType<CostUsageProviderRowsPayload>("CostUsageProviderRowsPayload");
    qRegisterMetaType<CostUsageDetailsRowsPayload>("CostUsageDetailsRowsPayload");
    qRegisterMetaType<CostUsageRefreshPayload>("CostUsageRefreshPayload");
    qRegisterMetaType<CostUsageProviderDetailPayload>("CostUsageProviderDetailPayload");
    qRegisterMetaType<ProviderFetchResult>("ProviderFetchResult");
    qRegisterMetaType<CredentialCacheUpdatePayload>("CredentialCacheUpdatePayload");
    qRegisterMetaType<ProviderRefreshPayload>("ProviderRefreshPayload");
    qRegisterMetaType<ProviderConnectionTestPayload>("ProviderConnectionTestPayload");
    qRegisterMetaType<ProviderStatusesPayload>("ProviderStatusesPayload");
    qRegisterMetaType<ProviderListPayload>("ProviderListPayload");
    qRegisterMetaType<ProviderDescriptorDataPayload>("ProviderDescriptorDataPayload");
    qRegisterMetaType<CodexCreditsFetcher::FetchResult>("CodexCreditsFetcher::FetchResult");
    qRegisterMetaType<CodexCreditsRefreshPayload>("CodexCreditsRefreshPayload");
    qRegisterMetaType<CredentialStatusPayload>("CredentialStatusPayload");
    qRegisterMetaType<CredentialPreloadPayload>("CredentialPreloadPayload");
    qRegisterMetaType<ProviderSecretResultPayload>("ProviderSecretResultPayload");
    qRegisterMetaType<ProviderLoginStartPayload>("ProviderLoginStartPayload");
    qRegisterMetaType<ProviderLoginPollPayload>("ProviderLoginPollPayload");

    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &UsageBackendWorker::valueJobFinished,
            this, &UsageBackend::jobFinished,
            Qt::QueuedConnection);
    m_workerThread.setObjectName(QStringLiteral("UsageBackend"));
    m_workerThread.start();
}

UsageBackend::~UsageBackend()
{
    m_workerThread.quit();
    m_workerThread.wait();
}

UsageBackendRequest UsageBackend::dispatchValueJob(const QString& kind,
                                                   int generation,
                                                   ValueJob job)
{
    UsageBackendRequest request;
    request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.kind = kind;
    request.generation = generation;

    QPointer<UsageBackendWorker> worker(m_worker);
    QMetaObject::invokeMethod(m_worker, [worker, request, job = std::move(job)]() mutable {
        if (!worker) {
            return;
        }
        worker->runValueJob(request, std::move(job));
    }, Qt::QueuedConnection);

    return request;
}

#include "UsageBackend.moc"
