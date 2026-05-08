#include <QtTest/QtTest>

#include "../src/providers/shared/ProviderStatusFetcher.h"

class tst_ProviderStatusFetcher : public QObject {
    Q_OBJECT

private slots:
    void statusPageEndpoint();
    void statusIndicatorMapping();
    void statusPageParse();
    void workspaceNoActiveIncidents();
    void workspaceIgnoresResolved();
    void workspaceCurrentlyAffectedMatch();
    void workspaceAffectedProductsFallback();
    void workspaceMultipleIncidentsPicksWorst();
    void workspaceStateMappings();
    void workspaceStatusImpactFallback();
    void workspaceSummaryCleaning();
    void workspaceMalformedJson();
    void openUrlPriority();
    void openUrlWorkspaceOnly();
    void buildPollTargetsStatuspage();
    void buildPollTargetsWorkspace();
    void buildPollTargetsLinkOnly();
};

void tst_ProviderStatusFetcher::statusPageEndpoint()
{
    QCOMPARE(ProviderStatusFetcher::statusEndpointFor(
        QStringLiteral("https://status.openai.com")),
        QStringLiteral("https://status.openai.com/api/v2/status.json"));

    QCOMPARE(ProviderStatusFetcher::statusEndpointFor(
        QStringLiteral("https://status.openai.com/")),
        QStringLiteral("https://status.openai.com/api/v2/status.json"));

    QVERIFY(ProviderStatusFetcher::statusEndpointFor(
        QStringLiteral("")).isEmpty());

    QVERIFY(ProviderStatusFetcher::statusEndpointFor(
        QStringLiteral("not-a-url")).isEmpty());
}

void tst_ProviderStatusFetcher::statusIndicatorMapping()
{
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("none")), QStringLiteral("ok"));
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("minor")), QStringLiteral("degraded"));
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("maintenance")), QStringLiteral("degraded"));
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("major")), QStringLiteral("outage"));
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("critical")), QStringLiteral("outage"));
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("unknown-indicator")), QStringLiteral("unknown"));
    QCOMPARE(ProviderStatusFetcher::mappedStatusFromIndicator(
        QStringLiteral("")), QStringLiteral("unknown"));
}

void tst_ProviderStatusFetcher::statusPageParse()
{
    QJsonObject json;
    QJsonObject status;
    status[QStringLiteral("indicator")] = QStringLiteral("minor");
    status[QStringLiteral("description")] = QStringLiteral("Elevated error rates");
    json[QStringLiteral("status")] = status;

    QJsonObject page;
    page[QStringLiteral("updated_at")] = QStringLiteral("2025-12-02T12:30:00.000Z");
    json[QStringLiteral("page")] = page;

    auto snap = ProviderStatusFetcher::parseStatuspageResponse(
        json, QStringLiteral("https://status.openai.com"));
    QCOMPARE(snap.state, QStringLiteral("degraded"));
    QCOMPARE(snap.description, QStringLiteral("Elevated error rates"));
    QCOMPARE(snap.source, QStringLiteral("statuspage"));
    QCOMPARE(snap.statusURL, QStringLiteral("https://status.openai.com"));
    QVERIFY(snap.updatedAt > 0);
}

void tst_ProviderStatusFetcher::workspaceNoActiveIncidents()
{
    QByteArray json = R"([
        {
            "begin": "2025-01-01T00:00:00+00:00",
            "end": "2025-01-01T01:00:00+00:00",
            "affected_products": [{"title":"Gemini","id":"npdyhgECDJ6tB66MxXyo"}]
        }
    ])";

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"),
        QStringLiteral("https://google.com/history"));
    QCOMPARE(snap.state, QStringLiteral("ok"));
    QCOMPARE(snap.source, QStringLiteral("workspace"));
}

void tst_ProviderStatusFetcher::workspaceIgnoresResolved()
{
    // An end field that is non-null means the incident is resolved
    QByteArray json = R"([
        {
            "begin": "2025-01-01T00:00:00+00:00",
            "end": "2025-01-01T01:00:00+00:00",
            "severity": "high",
            "affected_products": [{"title":"Gemini","id":"npdyhgECDJ6tB66MxXyo"}],
            "most_recent_update": {
                "status": "SERVICE_OUTAGE",
                "text": "Major outage"
            }
        }
    ])";

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"),
        QStringLiteral("https://google.com/history"));
    QCOMPARE(snap.state, QStringLiteral("ok"));
}

void tst_ProviderStatusFetcher::workspaceCurrentlyAffectedMatch()
{
    QByteArray json = R"([
        {
            "begin": "2025-12-02T09:00:00+00:00",
            "end": null,
            "severity": "medium",
            "currently_affected_products": [
                {"title":"Gemini","id":"npdyhgECDJ6tB66MxXyo"}
            ],
            "most_recent_update": {
                "when": "2025-12-02T12:30:00+00:00",
                "status": "SERVICE_DISRUPTION",
                "text": "Gemini API experiencing elevated latency"
            }
        }
    ])";

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"),
        QStringLiteral("https://google.com/history"));
    QCOMPARE(snap.state, QStringLiteral("outage"));
    QCOMPARE(snap.source, QStringLiteral("workspace"));
    QVERIFY(snap.description.contains(QStringLiteral("Gemini")));
}

void tst_ProviderStatusFetcher::workspaceAffectedProductsFallback()
{
    QByteArray json = R"([
        {
            "begin": "2025-12-02T09:00:00+00:00",
            "end": null,
            "severity": "low",
            "affected_products": [
                {"title":"Gemini","id":"npdyhgECDJ6tB66MxXyo"}
            ],
            "most_recent_update": {
                "status": "SERVICE_INFORMATION",
                "text": "Scheduled maintenance"
            }
        }
    ])";

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"),
        QStringLiteral("https://google.com/history"));
    QCOMPARE(snap.state, QStringLiteral("degraded"));
}

void tst_ProviderStatusFetcher::workspaceMultipleIncidentsPicksWorst()
{
    QByteArray json = R"([
        {
            "begin": "2025-12-02T09:00:00+00:00",
            "end": null,
            "severity": "low",
            "currently_affected_products": [{"title":"Gemini","id":"npdyhgECDJ6tB66MxXyo"}],
            "most_recent_update": {
                "status": "SERVICE_INFORMATION",
                "text": "Minor notification"
            }
        },
        {
            "begin": "2025-12-02T10:00:00+00:00",
            "end": null,
            "severity": "high",
            "currently_affected_products": [{"title":"Gemini","id":"npdyhgECDJ6tB66MxXyo"}],
            "most_recent_update": {
                "when": "2025-12-02T12:30:00+00:00",
                "status": "SERVICE_OUTAGE",
                "text": "Complete outage"
            }
        }
    ])";

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"),
        QStringLiteral("https://google.com/history"));
    QCOMPARE(snap.state, QStringLiteral("outage"));
    QVERIFY(snap.description.contains(QStringLiteral("Complete outage")));
}

void tst_ProviderStatusFetcher::workspaceStateMappings()
{
    // SERVICE_DISRUPTION -> outage (via provider)
    QByteArray json1 = R"([{
        "begin": "2025-01-01T00:00:00+00:00",
        "end": null,
        "currently_affected_products": [{"id":"npdyhgECDJ6tB66MxXyo"}],
        "most_recent_update": {"status": "SERVICE_DISRUPTION", "text": "test"}
    }])";
    auto snap1 = ProviderStatusFetcher::parseWorkspaceResponse(
        json1, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QCOMPARE(snap1.state, QStringLiteral("outage"));

    // SERVICE_OUTAGE -> outage
    QByteArray json2 = R"([{
        "begin": "2025-01-01T00:00:00+00:00",
        "end": null,
        "currently_affected_products": [{"id":"npdyhgECDJ6tB66MxXyo"}],
        "most_recent_update": {"status": "SERVICE_OUTAGE", "text": "test"}
    }])";
    auto snap2 = ProviderStatusFetcher::parseWorkspaceResponse(
        json2, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QCOMPARE(snap2.state, QStringLiteral("outage"));

    // SERVICE_MAINTENANCE -> degraded
    QByteArray json3 = R"([{
        "begin": "2025-01-01T00:00:00+00:00",
        "end": null,
        "currently_affected_products": [{"id":"npdyhgECDJ6tB66MxXyo"}],
        "most_recent_update": {"status": "SERVICE_MAINTENANCE", "text": "test"}
    }])";
    auto snap3 = ProviderStatusFetcher::parseWorkspaceResponse(
        json3, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QCOMPARE(snap3.state, QStringLiteral("degraded"));

    // Unknown status with severity=high -> outage
    QByteArray json4 = R"([{
        "begin": "2025-01-01T00:00:00+00:00",
        "end": null,
        "severity": "high",
        "currently_affected_products": [{"id":"npdyhgECDJ6tB66MxXyo"}],
        "most_recent_update": {"text": "test"}
    }])";
    auto snap4 = ProviderStatusFetcher::parseWorkspaceResponse(
        json4, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QCOMPARE(snap4.state, QStringLiteral("outage"));
}

void tst_ProviderStatusFetcher::workspaceStatusImpactFallback()
{
    QByteArray json = R"([{
        "begin": "2025-01-01T00:00:00+00:00",
        "end": null,
        "status_impact": "service_outage",
        "severity": "low",
        "currently_affected_products": [{"id":"npdyhgECDJ6tB66MxXyo"}],
        "most_recent_update": {"text": "Root impact should win over low severity"}
    }])";

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QCOMPARE(snap.state, QStringLiteral("outage"));
}

void tst_ProviderStatusFetcher::workspaceSummaryCleaning()
{
    QJsonObject update;
    update[QStringLiteral("text")] = QStringLiteral(
        "**Summary**\nGemini API is experiencing issues.\n\n"
        "**Description**\nWe are investigating.\n- Users may see errors.");
    update[QStringLiteral("status")] = QStringLiteral("SERVICE_DISRUPTION");

    QByteArray json = QJsonDocument(QJsonArray{
        QJsonObject{
            {QStringLiteral("begin"), QStringLiteral("2025-01-01T00:00:00+00:00")},
            {QStringLiteral("severity"), QStringLiteral("medium")},
            {QStringLiteral("currently_affected_products"), QJsonArray{
                QJsonObject{{QStringLiteral("id"), QStringLiteral("npdyhgECDJ6tB66MxXyo")}}
            }},
            {QStringLiteral("most_recent_update"), update}
        }
    }).toJson(QJsonDocument::Compact);

    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QVERIFY(!snap.description.contains(QStringLiteral("**Summary**")));
    QVERIFY(!snap.description.contains(QStringLiteral("**Description**")));
    QVERIFY(!snap.description.contains(QStringLiteral("- ")));
    QVERIFY(snap.description.contains(QStringLiteral("Gemini")));
}

void tst_ProviderStatusFetcher::workspaceMalformedJson()
{
    QByteArray json = QByteArrayLiteral("not valid json");
    auto snap = ProviderStatusFetcher::parseWorkspaceResponse(
        json, QStringLiteral("npdyhgECDJ6tB66MxXyo"), {});
    QCOMPARE(snap.state, QStringLiteral("unknown"));
    QCOMPARE(snap.source, QStringLiteral("workspace"));
}

void tst_ProviderStatusFetcher::openUrlPriority()
{
    // linkURL wins over pageURL
    QCOMPARE(ProviderStatusFetcher::openURL(
        QStringLiteral("https://status.alibabacloud.com"),
        QStringLiteral("https://status.aliyun.com"),
        {}),
        QStringLiteral("https://status.aliyun.com"));

    // pageURL when no linkURL
    QCOMPARE(ProviderStatusFetcher::openURL(
        QStringLiteral("https://status.openai.com"),
        {}, {}),
        QStringLiteral("https://status.openai.com"));

    // empty when all empty
    QVERIFY(ProviderStatusFetcher::openURL({}, {}, {}).isEmpty());
}

void tst_ProviderStatusFetcher::openUrlWorkspaceOnly()
{
    QString url = ProviderStatusFetcher::openURL(
        {}, {}, QStringLiteral("npdyhgECDJ6tB66MxXyo"));
    QVERIFY(url.contains(QStringLiteral("appsstatus")));
    QVERIFY(url.contains(QStringLiteral("npdyhgECDJ6tB66MxXyo")));
    QVERIFY(url.contains(QStringLiteral("history")));
}

void tst_ProviderStatusFetcher::buildPollTargetsStatuspage()
{
    auto targets = ProviderStatusFetcher::buildPollTargets(
        {QStringLiteral("codex")},
        QStringLiteral("https://status.openai.com"),
        {}, {});
    QCOMPARE(targets.size(), 1);
    QCOMPARE(targets[0].providerId, QStringLiteral("codex"));
    QCOMPARE(targets[0].source, ProviderStatusSource::Statuspage);
    QVERIFY(targets[0].requestURL.toString().contains(QStringLiteral("api/v2/status.json")));
}

void tst_ProviderStatusFetcher::buildPollTargetsWorkspace()
{
    auto targets = ProviderStatusFetcher::buildPollTargets(
        {QStringLiteral("gemini")},
        {}, {},
        QStringLiteral("npdyhgECDJ6tB66MxXyo"));
    QCOMPARE(targets.size(), 1);
    QCOMPARE(targets[0].providerId, QStringLiteral("gemini"));
    QCOMPARE(targets[0].source, ProviderStatusSource::GoogleWorkspace);
    QVERIFY(targets[0].requestURL.toString().contains(QStringLiteral("incidents.json")));
}

void tst_ProviderStatusFetcher::buildPollTargetsLinkOnly()
{
    auto targets = ProviderStatusFetcher::buildPollTargets(
        {QStringLiteral("deepseek")},
        {}, {}, {});
    QVERIFY(targets.isEmpty());
}

QTEST_MAIN(tst_ProviderStatusFetcher)
#include "tst_ProviderStatusFetcher.moc"
