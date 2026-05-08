#include "ProviderStatusFetcher.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>

namespace {

const QString kWorkspaceFeedURL = QStringLiteral(
    "https://www.google.com/appsstatus/dashboard/incidents.json");

const QString kWorkspaceHistoryBase = QStringLiteral(
    "https://www.google.com/appsstatus/dashboard/products/%1/history");

} // namespace

QVariantMap ProviderStatusSnapshot::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("state")] = state;
    if (!description.isEmpty()) map[QStringLiteral("description")] = description;
    if (!source.isEmpty()) map[QStringLiteral("source")] = source;
    if (!statusURL.isEmpty()) map[QStringLiteral("statusURL")] = statusURL;
    if (updatedAt > 0) map[QStringLiteral("updatedAt")] = updatedAt;
    return map;
}

QString ProviderStatusFetcher::statusEndpointFor(const QString& statusPageURL)
{
    QUrl url(statusPageURL);
    if (!url.isValid() || url.host().isEmpty()) return {};
    url.setPath(QStringLiteral("/api/v2/status.json"));
    url.setQuery(QString());
    url.setFragment({});
    return url.toString();
}

QString ProviderStatusFetcher::mappedStatusFromIndicator(const QString& indicator)
{
    if (indicator == QStringLiteral("none")) return QStringLiteral("ok");
    if (indicator == QStringLiteral("minor") || indicator == QStringLiteral("maintenance"))
        return QStringLiteral("degraded");
    if (indicator == QStringLiteral("major") || indicator == QStringLiteral("critical"))
        return QStringLiteral("outage");
    return QStringLiteral("unknown");
}

QVector<ProviderStatusPollTarget> ProviderStatusFetcher::buildPollTargets(
    const QVector<QString>& providerIDs,
    const QString& statusPageURL,
    const QString& statusLinkURL,
    const QString& statusWorkspaceProductID)
{
    QVector<ProviderStatusPollTarget> targets;
    for (const auto& id : providerIDs) {
        const QString spl = statusPageURL.trimmed();
        const QString swid = statusWorkspaceProductID.trimmed();

        if (!spl.isEmpty()) {
            const QString endpoint = statusEndpointFor(spl);
            if (!endpoint.isEmpty()) {
                ProviderStatusPollTarget target;
                target.providerId = id;
                target.source = ProviderStatusSource::Statuspage;
                target.requestURL = QUrl(endpoint);
                target.statusPageURL = spl;
                target.statusLinkURL = statusLinkURL.trimmed();
                target.workspaceProductID = swid;
                targets.append(target);
            }
        } else if (!swid.isEmpty()) {
            ProviderStatusPollTarget target;
            target.providerId = id;
            target.source = ProviderStatusSource::GoogleWorkspace;
            target.requestURL = QUrl(kWorkspaceFeedURL);
            target.statusPageURL = {};
            target.statusLinkURL = statusLinkURL.trimmed();
            target.workspaceProductID = swid;
            targets.append(target);
        }
    }
    return targets;
}

ProviderStatusSnapshot ProviderStatusFetcher::parseStatuspageResponse(
    const QJsonObject& json,
    const QString& statusPageURL)
{
    ProviderStatusSnapshot snap;
    snap.source = QStringLiteral("statuspage");
    snap.statusURL = statusPageURL;
    snap.updatedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();

    QJsonObject status = json.value(QStringLiteral("status")).toObject();
    snap.state = mappedStatusFromIndicator(status.value(QStringLiteral("indicator")).toString());
    snap.description = status.value(QStringLiteral("description")).toString().trimmed();

    QJsonObject page = json.value(QStringLiteral("page")).toObject();
    QString updatedAtStr = page.value(QStringLiteral("updated_at")).toString();
    if (!updatedAtStr.isEmpty()) {
        QDateTime dt = QDateTime::fromString(updatedAtStr, Qt::ISODateWithMs);
        if (!dt.isValid()) dt = QDateTime::fromString(updatedAtStr, Qt::ISODate);
        if (dt.isValid()) snap.updatedAt = dt.toMSecsSinceEpoch();
    }

    return snap;
}

QString ProviderStatusFetcher::workspaceStateFromStatus(const QString& status, const QString& severity)
{
    const QString normalizedStatus = status.trimmed().toUpper();
    const QString normalizedSeverity = severity.trimmed().toLower();

    if (normalizedStatus == QStringLiteral("AVAILABLE")) return QStringLiteral("ok");
    if (normalizedStatus == QStringLiteral("SERVICE_INFORMATION")) return QStringLiteral("degraded");
    if (normalizedStatus == QStringLiteral("SERVICE_DISRUPTION")) return QStringLiteral("outage");
    if (normalizedStatus == QStringLiteral("SERVICE_OUTAGE")) return QStringLiteral("outage");
    if (normalizedStatus == QStringLiteral("SERVICE_MAINTENANCE")) return QStringLiteral("degraded");
    if (normalizedStatus == QStringLiteral("SCHEDULED_MAINTENANCE")) return QStringLiteral("degraded");

    if (normalizedSeverity == QStringLiteral("low")) return QStringLiteral("degraded");
    if (normalizedSeverity == QStringLiteral("medium")) return QStringLiteral("outage");
    if (normalizedSeverity == QStringLiteral("high")) return QStringLiteral("outage");

    return QStringLiteral("degraded");
}

QString ProviderStatusFetcher::workspaceSummary(const QJsonObject& mostRecentUpdate)
{
    QString text = mostRecentUpdate.value(QStringLiteral("text")).toString();
    if (text.isEmpty()) return {};

    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QChar('\r'), QChar('\n'));

    const QStringList lines = text.split(QLatin1Char('\n'));
    QStringList cleanLines;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty()) continue;

        if (line == QStringLiteral("**Summary**") || line == QStringLiteral("**Description**") ||
            line == QStringLiteral("Summary") || line == QStringLiteral("Description")) {
            continue;
        }

        static const QRegularExpression boldRe(QStringLiteral("\\*\\*(.*?)\\*\\*"));
        line.replace(boldRe, QStringLiteral("\\1"));

        static const QRegularExpression linkRe(QStringLiteral("\\[(.*?)\\]\\(.*?\\)"));
        line.replace(linkRe, QStringLiteral("\\1"));

        if (line.startsWith(QStringLiteral("- "))) {
            line = line.mid(2);
        }

        cleanLines.append(line);
    }

    QString result = cleanLines.join(QLatin1Char(' '));
    constexpr int kMaxLength = 240;
    if (result.size() > kMaxLength) {
        result = result.left(kMaxLength).trimmed();
    }
    return result;
}

int ProviderStatusFetcher::statusSeverity(const QString& state)
{
    if (state == QStringLiteral("ok")) return 0;
    if (state == QStringLiteral("degraded")) return 2;
    if (state == QStringLiteral("outage")) return 4;
    return 2;
}

ProviderStatusSnapshot ProviderStatusFetcher::parseWorkspaceResponse(
    const QByteArray& rawData,
    const QString& workspaceProductID,
    const QString& statusLinkURL)
{
    ProviderStatusSnapshot snap;
    snap.source = QStringLiteral("workspace");
    snap.statusURL = statusLinkURL;
    snap.state = QStringLiteral("ok");

    if (workspaceProductID.isEmpty()) {
        snap.state = QStringLiteral("unknown");
        snap.description = QStringLiteral("missing workspace product ID");
        return snap;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(rawData, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        snap.state = QStringLiteral("unknown");
        snap.description = QStringLiteral("invalid workspace feed: %1").arg(error.errorString());
        return snap;
    }

    const QJsonArray incidents = doc.array();
    ProviderStatusSnapshot worst;
    worst.state = QStringLiteral("ok");
    worst.source = QStringLiteral("workspace");
    worst.statusURL = statusLinkURL;
    int worstSev = 0;

    for (const auto& incidentVal : incidents) {
        if (!incidentVal.isObject()) continue;
        const QJsonObject incident = incidentVal.toObject();

        const QJsonValue endVal = incident.value(QStringLiteral("end"));
        if (!endVal.isNull() && !endVal.isUndefined()) continue;

        bool matchesProduct = false;
        const QJsonArray curAffected = incident.value(QStringLiteral("currently_affected_products")).toArray();
        if (!curAffected.isEmpty()) {
            for (const auto& prod : curAffected) {
                if (prod.toObject().value(QStringLiteral("id")).toString() == workspaceProductID) {
                    matchesProduct = true;
                    break;
                }
            }
        } else {
            const QJsonArray affected = incident.value(QStringLiteral("affected_products")).toArray();
            for (const auto& prod : affected) {
                if (prod.toObject().value(QStringLiteral("id")).toString() == workspaceProductID) {
                    matchesProduct = true;
                    break;
                }
            }
        }

        if (!matchesProduct) continue;

        const QJsonObject mostRecent = incident.value(QStringLiteral("most_recent_update")).toObject();
        QString status = mostRecent.value(QStringLiteral("status")).toString();
        if (status.trimmed().isEmpty()) {
            status = incident.value(QStringLiteral("status_impact")).toString();
        }
        const QString severity = incident.value(QStringLiteral("severity")).toString();
        const QString state = workspaceStateFromStatus(status, severity);

        int sev = statusSeverity(state);
        qint64 updatedAt = 0;
        const QString whenStr = mostRecent.value(QStringLiteral("when")).toString();
        if (!whenStr.isEmpty()) {
            QDateTime dt = QDateTime::fromString(whenStr, Qt::ISODateWithMs);
            if (!dt.isValid()) dt = QDateTime::fromString(whenStr, Qt::ISODate);
            if (dt.isValid()) updatedAt = dt.toMSecsSinceEpoch();
        }

        if (sev > worstSev || (sev == worstSev && updatedAt > snap.updatedAt)) {
            worstSev = sev;
            snap.state = state;
            snap.description = workspaceSummary(mostRecent);
            snap.updatedAt = updatedAt;
        }
    }

    if (snap.state.isEmpty()) snap.state = QStringLiteral("ok");
    if (snap.updatedAt == 0) snap.updatedAt = QDateTime::currentDateTime().toMSecsSinceEpoch();
    return snap;
}

QString ProviderStatusFetcher::openURL(const QString& statusPageURL,
                                        const QString& statusLinkURL,
                                        const QString& statusWorkspaceProductID)
{
    if (!statusLinkURL.trimmed().isEmpty()) return statusLinkURL.trimmed();
    if (!statusPageURL.trimmed().isEmpty()) return statusPageURL.trimmed();
    if (!statusWorkspaceProductID.trimmed().isEmpty())
        return kWorkspaceHistoryBase.arg(statusWorkspaceProductID.trimmed());
    return {};
}
