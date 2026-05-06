#pragma once

#include "UsageBackendTypes.h"

#include <QObject>
#include <QThread>
#include <QVariant>

#include <functional>

class UsageBackendWorker;

class UsageBackend : public QObject {
    Q_OBJECT

public:
    using ValueJob = std::function<QVariant()>;

    explicit UsageBackend(QObject* parent = nullptr);
    ~UsageBackend() override;

    UsageBackendRequest dispatchValueJob(const QString& kind,
                                         int generation,
                                         ValueJob job);

signals:
    void jobFinished(const UsageBackendResult& result);

private:
    QThread m_workerThread;
    UsageBackendWorker* m_worker = nullptr;
};
