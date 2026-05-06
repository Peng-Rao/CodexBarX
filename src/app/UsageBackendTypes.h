#pragma once

#include "../models/CostUsageReport.h"
#include "../models/UsageSnapshot.h"
#include "../providers/codex/CodexCreditsFetcher.h"
#include "../providers/ProviderFetchResult.h"

#include <QHash>
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

struct CostUsageViewDataPayload {
    QVariantMap costData;
    QVariantList providerList;
};

struct CostUsageRefreshPayload {
    CostUsageSnapshot combined;
    QHash<QString, CostUsageSnapshot> perProvider;
    QVector<ProviderCostUsageSnapshot> allProviders;
};

struct ProviderRefreshPayload {
    QString providerId;
    ProviderFetchResult fetchResult;
};

struct ProviderConnectionTestPayload {
    QString providerId;
    ProviderFetchResult fetchResult;
    qint64 startedAt = 0;
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
Q_DECLARE_METATYPE(CostUsageViewDataPayload)
Q_DECLARE_METATYPE(CostUsageRefreshPayload)
Q_DECLARE_METATYPE(ProviderFetchResult)
Q_DECLARE_METATYPE(ProviderRefreshPayload)
Q_DECLARE_METATYPE(ProviderConnectionTestPayload)
Q_DECLARE_METATYPE(ProviderStatusesPayload)
Q_DECLARE_METATYPE(ProviderListPayload)
Q_DECLARE_METATYPE(ProviderDescriptorDataPayload)
Q_DECLARE_METATYPE(CodexCreditsFetcher::FetchResult)
Q_DECLARE_METATYPE(CodexCreditsRefreshPayload)
Q_DECLARE_METATYPE(CredentialStatusPayload)
Q_DECLARE_METATYPE(ProviderSecretResultPayload)
Q_DECLARE_METATYPE(ProviderLoginStartPayload)
Q_DECLARE_METATYPE(ProviderLoginPollPayload)
