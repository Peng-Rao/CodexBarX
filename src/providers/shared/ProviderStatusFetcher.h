#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QVariantMap>

struct ProviderStatusSnapshot {
    QString state;
    QString description;
    QString source;
    QString statusURL;
    qint64 updatedAt = 0;

    QVariantMap toVariantMap() const;
};

enum class ProviderStatusSource {
    Statuspage,
    GoogleWorkspace
};

struct ProviderStatusPollTarget {
    QString providerId;
    ProviderStatusSource source;
    QUrl requestURL;
    QString statusPageURL;
    QString statusLinkURL;
    QString workspaceProductID;
};

class ProviderStatusFetcher {
public:
    static QVector<ProviderStatusPollTarget> buildPollTargets(
        const QVector<QString>& providerIDs,
        const QString& statusPageURL,
        const QString& statusLinkURL,
        const QString& statusWorkspaceProductID);

    static QString statusEndpointFor(const QString& statusPageURL);

    static QString mappedStatusFromIndicator(const QString& indicator);

    static ProviderStatusSnapshot parseStatuspageResponse(
        const QJsonObject& json,
        const QString& statusPageURL);

    static ProviderStatusSnapshot parseWorkspaceResponse(
        const QByteArray& rawData,
        const QString& workspaceProductID,
        const QString& statusLinkURL);

    static QString openURL(const QString& statusPageURL,
                           const QString& statusLinkURL,
                           const QString& statusWorkspaceProductID);

private:
    static QString workspaceStateFromStatus(const QString& status, const QString& severity);
    static QString workspaceSummary(const QJsonObject& mostRecentUpdate);
    static int statusSeverity(const QString& state);
};
