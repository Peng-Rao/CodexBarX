#pragma once

#include "../models/CostUsageReport.h"
#include "../models/UsageSnapshot.h"
#include "../providers/codex/CodexCreditsFetcher.h"
#include "../providers/ProviderFetchResult.h"

#include <QHash>
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

struct UsageBackendRequest {
    QString requestId;
    QString kind;
    int generation = 0;
};

struct UsageBackendResult {
    QString requestId;
    QString kind;
    int generation = 0;
    bool success = true;
    QString message;
    QVariant payload;
};

struct CostUsageSummaryPayload {
    QVariantMap costData;
};

struct CostUsageProviderRowsPayload {
    QVariantList providerList;
};

struct CostUsageDetailsRowsPayload {
    QVariantList detailsRows;
    int tokenProviderCount = 0;
};

struct CostUsageRefreshPayload {
    CostUsageSnapshot combined;
    QHash<QString, CostUsageSnapshot> perProvider;
    QVector<ProviderCostUsageSnapshot> allProviders;
};

struct CostUsageProviderDetailPayload {
    QString providerId;
    QVariantMap detail;
};

struct CredentialCacheUpdatePayload {
    QString target;
    bool exists = false;
    QByteArray data;
};

struct ProviderRefreshPayload {
    QString providerId;
    ProviderFetchResult fetchResult;
    QVector<CredentialCacheUpdatePayload> credentialUpdates;
};

struct ProviderConnectionTestPayload {
    QString providerId;
    ProviderFetchResult fetchResult;
    qint64 startedAt = 0;
    QVector<CredentialCacheUpdatePayload> credentialUpdates;
};

struct ProviderStatusesPayload {
    QHash<QString, QVariantMap> statuses;
};

struct ProviderListPayload {
    QVariantList providers;
};

struct ProviderDescriptorDataPayload {
    QString providerId;
    QVariantMap descriptor;
};

struct CodexCreditsRefreshPayload {
    CodexCreditsFetcher::FetchResult result;
};

struct CredentialStatusPayload {
    QString providerId;
    QString key;
    QString target;
    bool exists = false;
};

struct CredentialPreloadPayload {
    QVector<CredentialCacheUpdatePayload> updates;
};

struct ProviderSecretResultPayload {
    QString providerId;
    QString key;
    QString target;
    bool success = false;
    bool removed = false;
};

struct ProviderLoginStartPayload {
    QString providerId;
    bool success = false;
    QString message;
    QString deviceCode;
    QString userCode;
    QString verificationUri;
    int interval = 5;
    int expiresIn = 900;
};

struct ProviderLoginPollPayload {
    QString providerId;
    QString state;
    QString message;
    bool triggerConnectionTest = false;
};

Q_DECLARE_METATYPE(UsageBackendRequest)
Q_DECLARE_METATYPE(UsageBackendResult)
Q_DECLARE_METATYPE(CostUsageSummaryPayload)
Q_DECLARE_METATYPE(CostUsageProviderRowsPayload)
Q_DECLARE_METATYPE(CostUsageDetailsRowsPayload)
Q_DECLARE_METATYPE(CostUsageRefreshPayload)
Q_DECLARE_METATYPE(CostUsageProviderDetailPayload)
Q_DECLARE_METATYPE(ProviderFetchResult)
Q_DECLARE_METATYPE(CredentialCacheUpdatePayload)
Q_DECLARE_METATYPE(ProviderRefreshPayload)
Q_DECLARE_METATYPE(ProviderConnectionTestPayload)
Q_DECLARE_METATYPE(ProviderStatusesPayload)
Q_DECLARE_METATYPE(ProviderListPayload)
Q_DECLARE_METATYPE(ProviderDescriptorDataPayload)
Q_DECLARE_METATYPE(CodexCreditsFetcher::FetchResult)
Q_DECLARE_METATYPE(CodexCreditsRefreshPayload)
Q_DECLARE_METATYPE(CredentialStatusPayload)
Q_DECLARE_METATYPE(CredentialPreloadPayload)
Q_DECLARE_METATYPE(ProviderSecretResultPayload)
Q_DECLARE_METATYPE(ProviderLoginStartPayload)
Q_DECLARE_METATYPE(ProviderLoginPollPayload)
