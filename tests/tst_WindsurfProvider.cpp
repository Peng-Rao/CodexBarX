#include <QtTest/QtTest>

#include "../src/providers/windsurf/WindsurfProvider.h"
#include "../src/providers/windsurf/WindsurfDevinSessionImporter.h"
#include "../src/providers/ProviderPipeline.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QUuid>

namespace {

struct WindsurfSessionImporterOverrideGuard {
    ~WindsurfSessionImporterOverrideGuard()
    {
        WindsurfDevinSessionImporter::clearOverrides();
    }
};

QVector<WindsurfDevinSessionInfo> noImportedSessions(const ProviderFetchContext&)
{
    return {};
}

} // namespace

class tst_WindsurfProvider : public QObject {
    Q_OBJECT

private slots:
    void providerMetadataAndSettings();
    void parseFullCachedPlanInfo();
    void parseUsageCountsFallback();
    void readDatabaseBlobs();
    void parseManualSessionInput();
    void protobufCodec();
    void filtersSourceModes();
    void webConfigErrors();
    void sessionImporterDecodedValue();
    void sessionImporterFromStorage();
    void sessionImporterIncompleteStorage();
    void sessionImporterDeduplicate();
    void sessionImporterBrowserOrder();
    void webAutoSessionAvailable();
    void webAutoSessionNoSessionsError();
    void webAutoSourceOffError();

private:
    static QString makeDatabase(const QByteArray& jsonData, QTemporaryDir& dir);
    static QByteArray makePlanStatusResponse(const QString& planName,
                                             int dailyRemaining,
                                             int weeklyRemaining,
                                             qint64 planEndUnix,
                                             qint64 dailyResetUnix,
                                             qint64 weeklyResetUnix);
    static QByteArray varint(quint64 value);
    static QByteArray fieldKey(int number, int wireType);
    static QByteArray lengthDelimitedField(int number, const QByteArray& value);
    static QByteArray stringField(int number, const QString& value);
    static QByteArray varintField(int number, quint64 value);
    static QByteArray messageField(int number, const QByteArray& value);
    static QByteArray timestamp(qint64 seconds);
};

void tst_WindsurfProvider::providerMetadataAndSettings()
{
    WindsurfProvider provider;
    QCOMPARE(provider.id(), QStringLiteral("windsurf"));
    QCOMPARE(provider.displayName(), QStringLiteral("Windsurf"));
    QCOMPARE(provider.sessionLabel(), QStringLiteral("Daily"));
    QCOMPARE(provider.weeklyLabel(), QStringLiteral("Weekly"));
    QCOMPARE(provider.brandColor(), QStringLiteral("#34E8BB"));
    QCOMPARE(provider.dashboardURL(), QStringLiteral("https://windsurf.com/subscription/usage"));
    QCOMPARE(provider.supportedSourceModes(), QVector<QString>({"auto", "web", "cli"}));

    auto settings = provider.settingsDescriptors();
    QCOMPARE(settings.size(), 3);
    QCOMPARE(settings[0].key, QStringLiteral("sourceMode"));
    QCOMPARE(settings[0].defaultValue.toString(), QStringLiteral("auto"));
    QCOMPARE(settings[1].key, QStringLiteral("cookieSource"));
    QCOMPARE(settings[1].defaultValue.toString(), QStringLiteral("auto"));
    QCOMPARE(settings[1].options.size(), 3);
    QCOMPARE(settings[1].options[0].value, QStringLiteral("auto"));
    QCOMPARE(settings[1].options[1].value, QStringLiteral("manual"));
    QCOMPARE(settings[1].options[2].value, QStringLiteral("off"));
    QVERIFY(settings[2].sensitive);
    QCOMPARE(settings[2].credentialTarget, QStringLiteral("com.codexbarx.session.windsurf"));
}

void tst_WindsurfProvider::parseFullCachedPlanInfo()
{
    const QString json = R"({
      "planName": "Pro",
      "startTimestamp": 1771610750000,
      "endTimestamp": 1774029950000,
      "usage": {
        "messages": 50000,
        "usedMessages": 35650,
        "remainingMessages": 14350,
        "flowActions": 150000,
        "usedFlowActions": 0,
        "remainingFlowActions": 150000
      },
      "quotaUsage": {
        "dailyRemainingPercent": 9,
        "weeklyRemainingPercent": 54,
        "dailyResetAtUnix": 1774080000,
        "weeklyResetAtUnix": 1774166400
      }
    })";

    ProviderFetchResult result = WindsurfLocalStrategy::parseCachedPlanInfoJson(json);
    QVERIFY(result.success);
    QVERIFY(result.usage.primary.has_value());
    QCOMPARE(result.usage.primary->usedPercent, 91.0);
    QVERIFY(result.usage.primary->resetsAt.has_value());
    QVERIFY(result.usage.secondary.has_value());
    QCOMPARE(result.usage.secondary->usedPercent, 46.0);
    QVERIFY(result.usage.identity.has_value());
    QCOMPARE(result.usage.identity->providerID.value(), UsageProvider::windsurf);
    QCOMPARE(result.usage.identity->loginMethod.value(), QStringLiteral("Pro"));
    QVERIFY(result.usage.identity->accountOrganization.has_value());
}

void tst_WindsurfProvider::parseUsageCountsFallback()
{
    const QString json = R"({
      "planName": "Pro",
      "usage": {
        "messages": 50000,
        "usedMessages": 1200,
        "remainingMessages": 48800,
        "flowActions": 150000,
        "remainingFlowActions": 150000
      }
    })";

    ProviderFetchResult result = WindsurfLocalStrategy::parseCachedPlanInfoJson(json);
    QVERIFY(result.success);
    QVERIFY(result.usage.primary.has_value());
    QCOMPARE(result.usage.primary->usedPercent, 2.4);
    QCOMPARE(result.usage.primary->resetDescription.value(), QStringLiteral("1200 / 50000 messages"));
    QVERIFY(result.usage.secondary.has_value());
    QCOMPARE(result.usage.secondary->usedPercent, 0.0);
    QCOMPARE(result.usage.secondary->resetDescription.value(), QStringLiteral("0 / 150000 flow actions"));

    ProviderFetchResult inferred = WindsurfLocalStrategy::parseCachedPlanInfoJson(
        R"({"planName":"Pro","usage":{"messages":100,"remainingMessages":25}})");
    QVERIFY(inferred.success);
    QCOMPARE(inferred.usage.primary->usedPercent, 75.0);
    QCOMPARE(inferred.usage.primary->resetDescription.value(), QStringLiteral("75 / 100 messages"));
    QVERIFY(!inferred.usage.secondary.has_value());
}

void tst_WindsurfProvider::readDatabaseBlobs()
{
    {
        QTemporaryDir dir(QDir::currentPath() + QStringLiteral("/windsurf-utf8-XXXXXX"));
        QVERIFY(dir.isValid());
        const QString dbPath = makeDatabase(QByteArrayLiteral(R"({"planName":"UTF-8 Pro","quotaUsage":{"dailyRemainingPercent":20}})"), dir);
        QVERIFY(!dbPath.isEmpty());
        ProviderFetchResult result = WindsurfLocalStrategy::readDatabase(dbPath);
        QVERIFY(result.success);
        QCOMPARE(result.usage.identity->loginMethod.value(), QStringLiteral("UTF-8 Pro"));
        QCOMPARE(result.usage.primary->usedPercent, 80.0);
    }

    {
        QTemporaryDir dir(QDir::currentPath() + QStringLiteral("/windsurf-utf16-XXXXXX"));
        QVERIFY(dir.isValid());
        const QString json = QStringLiteral(R"({"planName":"UTF-16 Pro","quotaUsage":{"dailyRemainingPercent":75}})");
        const QByteArray utf16(reinterpret_cast<const char*>(json.utf16()),
                               json.size() * static_cast<int>(sizeof(char16_t)));
        const QString dbPath = makeDatabase(utf16, dir);
        QVERIFY(!dbPath.isEmpty());
        ProviderFetchResult result = WindsurfLocalStrategy::readDatabase(dbPath);
        QVERIFY(result.success);
        QCOMPARE(result.usage.identity->loginMethod.value(), QStringLiteral("UTF-16 Pro"));
        QCOMPARE(result.usage.primary->usedPercent, 25.0);
    }

    ProviderFetchResult missing = WindsurfLocalStrategy::readDatabase(
        QDir::currentPath() + QStringLiteral("/missing-windsurf-state.vscdb"));
    QVERIFY(!missing.success);
    QVERIFY(missing.errorMessage.contains(QStringLiteral("cache not found")));
}

void tst_WindsurfProvider::parseManualSessionInput()
{
    QString error;
    auto jsonAuth = WindsurfWebStrategy::parseManualSessionInput(R"({
      "devinSessionToken": "devin-session-token$abc",
      "devinAuth1Token": "auth1_xyz",
      "devinAccountId": "account-123",
      "devinPrimaryOrgId": "org-456"
    })", &error);
    QVERIFY2(jsonAuth.has_value(), qPrintable(error));
    QCOMPARE(jsonAuth->sessionToken, QStringLiteral("devin-session-token$abc"));
    QCOMPARE(jsonAuth->auth1Token, QStringLiteral("auth1_xyz"));
    QCOMPARE(jsonAuth->accountID, QStringLiteral("account-123"));
    QCOMPARE(jsonAuth->primaryOrgID, QStringLiteral("org-456"));

    auto kvAuth = WindsurfWebStrategy::parseManualSessionInput(
        "devin_session_token=token\n"
        "devin_auth1_token=auth1\n"
        "devin_account_id=account\n"
        "devin_primary_org_id=org",
        &error);
    QVERIFY2(kvAuth.has_value(), qPrintable(error));
    QCOMPARE(kvAuth->sessionToken, QStringLiteral("token"));
    QCOMPARE(kvAuth->auth1Token, QStringLiteral("auth1"));

    auto invalid = WindsurfWebStrategy::parseManualSessionInput(QStringLiteral("not a bundle"), &error);
    QVERIFY(!invalid.has_value());
    QVERIFY(error.contains(QStringLiteral("expected JSON")));
}

void tst_WindsurfProvider::protobufCodec()
{
    const QByteArray requestBody = WindsurfPlanStatusProtoCodec::encodeRequest(
        QStringLiteral("devin-session-token$abc"),
        true);
    QString error;
    auto request = WindsurfPlanStatusProtoCodec::decodeRequest(requestBody, &error);
    QVERIFY2(request.has_value(), qPrintable(error));
    QCOMPARE(request->authToken, QStringLiteral("devin-session-token$abc"));
    QVERIFY(request->includeTopUpStatus);

    const QByteArray responseBody = makePlanStatusResponse(
        QStringLiteral("Pro"),
        68,
        84,
        1777888000,
        1777900000,
        1778000000);
    auto status = WindsurfPlanStatusProtoCodec::decodeResponse(responseBody, &error);
    QVERIFY2(status.has_value(), qPrintable(error));
    QCOMPARE(status->planName, QStringLiteral("Pro"));
    QCOMPARE(status->dailyQuotaRemainingPercent.value(), 68);
    QCOMPARE(status->weeklyQuotaRemainingPercent.value(), 84);
    QCOMPARE(status->dailyQuotaResetAtUnix.value(), static_cast<qint64>(1777900000));

    UsageSnapshot snap = WindsurfWebStrategy::planStatusToUsageSnapshot(*status);
    QCOMPARE(snap.primary->usedPercent, 32.0);
    QCOMPARE(snap.secondary->usedPercent, 16.0);
    QCOMPARE(snap.identity->loginMethod.value(), QStringLiteral("Pro"));
}

void tst_WindsurfProvider::filtersSourceModes()
{
    ProviderPipeline pipeline;
    WindsurfProvider provider;
    ProviderFetchContext ctx;

    auto autoStrategies = pipeline.resolveStrategies(&provider, ctx);
    QCOMPARE(autoStrategies.size(), 2);
    QCOMPARE(autoStrategies[0]->id(), QStringLiteral("windsurf.web"));
    QCOMPARE(autoStrategies[1]->id(), QStringLiteral("windsurf.local"));
    qDeleteAll(autoStrategies);

    ctx.sourceMode = ProviderSourceMode::Web;
    auto webStrategies = pipeline.resolveStrategies(&provider, ctx);
    QCOMPARE(webStrategies.size(), 1);
    QCOMPARE(webStrategies[0]->id(), QStringLiteral("windsurf.web"));
    qDeleteAll(webStrategies);

    ctx.sourceMode = ProviderSourceMode::CLI;
    auto cliStrategies = pipeline.resolveStrategies(&provider, ctx);
    QCOMPARE(cliStrategies.size(), 1);
    QCOMPARE(cliStrategies[0]->id(), QStringLiteral("windsurf.local"));
    QCOMPARE(cliStrategies[0]->kind(), static_cast<int>(ProviderFetchKind::CLI));
    qDeleteAll(cliStrategies);
}

void tst_WindsurfProvider::sessionImporterDecodedValue()
{
    QCOMPARE(WindsurfDevinSessionImporter::decodedStorageValue(
        QStringLiteral("  plain-token  ")), QStringLiteral("plain-token"));

    QCOMPARE(WindsurfDevinSessionImporter::decodedStorageValue(
        QStringLiteral("\"quoted-token\"")), QStringLiteral("quoted-token"));

    QCOMPARE(WindsurfDevinSessionImporter::decodedStorageValue(
        QStringLiteral("'single-quoted'")), QStringLiteral("single-quoted"));

    QString jsonWrapped = QStringLiteral("\"json-wrapped-token\"");
    QCOMPARE(WindsurfDevinSessionImporter::decodedStorageValue(jsonWrapped),
             QStringLiteral("json-wrapped-token"));

    QCOMPARE(WindsurfDevinSessionImporter::decodedStorageValue(
        QStringLiteral("  token-with-$-and._chars  ")), QStringLiteral("token-with-$-and._chars"));

    QCOMPARE(WindsurfDevinSessionImporter::decodedStorageValue(
        QStringLiteral("")), QString());
}

void tst_WindsurfProvider::sessionImporterFromStorage()
{
    QHash<QString, QString> storage;
    storage[QStringLiteral("devin_session_token")] = QStringLiteral("sess-token");
    storage[QStringLiteral("devin_auth1_token")] = QStringLiteral("auth1-token");
    storage[QStringLiteral("devin_account_id")] = QStringLiteral("acct-123");
    storage[QStringLiteral("devin_primary_org_id")] = QStringLiteral("org-456");

    auto info = WindsurfDevinSessionImporter::sessionFromStorage(
        storage, QStringLiteral("Chrome Default"), QStringLiteral("/path/leveldb"));
    QVERIFY(info.has_value());
    QCOMPARE(info->session.sessionToken, QStringLiteral("sess-token"));
    QCOMPARE(info->session.auth1Token, QStringLiteral("auth1-token"));
    QCOMPARE(info->session.accountID, QStringLiteral("acct-123"));
    QCOMPARE(info->session.primaryOrgID, QStringLiteral("org-456"));
    QCOMPARE(info->sourceLabel, QStringLiteral("Chrome Default"));
    QCOMPARE(info->levelDBPath, QStringLiteral("/path/leveldb"));
}

void tst_WindsurfProvider::sessionImporterIncompleteStorage()
{
    QHash<QString, QString> storage;
    storage[QStringLiteral("devin_session_token")] = QStringLiteral("sess-token");
    // Missing auth1, account, org
    auto info = WindsurfDevinSessionImporter::sessionFromStorage(
        storage, QStringLiteral("Chrome Default"));
    QVERIFY(!info.has_value());

    storage.clear();
    storage[QStringLiteral("devin_session_token")] = QStringLiteral("sess");
    storage[QStringLiteral("devin_auth1_token")] = QStringLiteral("auth1");
    storage[QStringLiteral("devin_account_id")] = QStringLiteral("acct");
    // missing primary org
    auto info2 = WindsurfDevinSessionImporter::sessionFromStorage(
        storage, QStringLiteral("Label"));
    QVERIFY(!info2.has_value());
}

void tst_WindsurfProvider::sessionImporterDeduplicate()
{
    WindsurfDevinSessionInfo a;
    a.session.sessionToken = QStringLiteral("token-aaa");
    a.sourceLabel = QStringLiteral("Chrome Default");

    WindsurfDevinSessionInfo b;
    b.session.sessionToken = QStringLiteral("token-aaa"); // same token
    b.sourceLabel = QStringLiteral("Edge Default");

    WindsurfDevinSessionInfo c;
    c.session.sessionToken = QStringLiteral("token-bbb");
    c.sourceLabel = QStringLiteral("Chrome Profile 1");

    QVector<WindsurfDevinSessionInfo> sessions = {a, b, c};
    auto deduped = WindsurfDevinSessionImporter::deduplicateSessions(sessions);

    QCOMPARE(deduped.size(), 2);
    QCOMPARE(deduped[0].sourceLabel, QStringLiteral("Chrome Default"));
    QCOMPARE(deduped[0].session.sessionToken, QStringLiteral("token-aaa"));
    QCOMPARE(deduped[1].session.sessionToken, QStringLiteral("token-bbb"));
}

void tst_WindsurfProvider::sessionImporterBrowserOrder()
{
    auto pref = WindsurfDevinSessionImporter::preferredBrowsers();
    QCOMPARE(pref.size(), 1);
    QCOMPARE(pref[0], CookieImporter::Chrome);

    auto fallback = WindsurfDevinSessionImporter::fallbackBrowsersExcludingPreferred();
    QVERIFY(!fallback.contains(CookieImporter::Chrome));
    QVERIFY(fallback.size() >= 3);

    // Fallback should include Edge
    bool hasEdge = false;
    for (auto b : fallback) {
        if (b == CookieImporter::Edge) { hasEdge = true; break; }
    }
    QVERIFY(hasEdge);
}

void tst_WindsurfProvider::webAutoSessionAvailable()
{
    WindsurfWebStrategy strategy;
    ProviderFetchContext ctx;

    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("auto"));
    QVERIFY(strategy.isAvailable(ctx));

    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("manual"));
    QVERIFY(!strategy.isAvailable(ctx)); // no manual session configured

    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("off"));
    QVERIFY(!strategy.isAvailable(ctx));
}

void tst_WindsurfProvider::webAutoSessionNoSessionsError()
{
    WindsurfSessionImporterOverrideGuard guard;
    WindsurfDevinSessionImporter::setImportPreferredSessionsOverride(noImportedSessions);
    WindsurfDevinSessionImporter::setImportFallbackSessionsOverride(noImportedSessions);

    WindsurfWebStrategy strategy;
    ProviderFetchContext ctx;
    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("auto"));

    ProviderFetchResult result = strategy.fetchSync(ctx);
    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("No Windsurf web session")));
    QVERIFY(result.errorMessage.contains(QStringLiteral("Chromium")));
}

void tst_WindsurfProvider::webAutoSourceOffError()
{
    WindsurfWebStrategy strategy;
    ProviderFetchContext ctx;
    ctx.sourceMode = ProviderSourceMode::Web;
    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("off"));

    ProviderFetchResult result = strategy.fetchSync(ctx);
    QVERIFY(!result.success);
    QVERIFY(result.errorMessage.contains(QStringLiteral("disabled")));
}

void tst_WindsurfProvider::webConfigErrors()
{
    WindsurfWebStrategy strategy;
    ProviderFetchContext ctx;

    ctx.sourceMode = ProviderSourceMode::Auto;
    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("off"));
    QVERIFY(!strategy.isAvailable(ctx));

    ctx.sourceMode = ProviderSourceMode::Web;
    QVERIFY(strategy.isAvailable(ctx));
    ProviderFetchResult disabled = strategy.fetchSync(ctx);
    QVERIFY(!disabled.success);
    QVERIFY(disabled.errorMessage.contains(QStringLiteral("disabled")));

    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("manual"));
    ProviderFetchResult missing = strategy.fetchSync(ctx);
    QVERIFY(!missing.success);
    QVERIFY(missing.errorMessage.contains(QStringLiteral("not configured")));

    ctx.settings.set(QStringLiteral("cookieSource"), QStringLiteral("auto"));
    QVERIFY(strategy.isAvailable(ctx));
}

QString tst_WindsurfProvider::makeDatabase(const QByteArray& jsonData, QTemporaryDir& dir)
{
    const QString dbPath = dir.path() + QStringLiteral("/state.vscdb");
    {
        QFile placeholder(dbPath);
        if (!placeholder.open(QIODevice::WriteOnly)) {
            qWarning() << "Failed to create placeholder Windsurf DB" << dbPath << placeholder.errorString();
            return {};
        }
    }
    const QString connectionName = QStringLiteral("windsurf-test-%1").arg(QUuid::createUuid().toString(QUuid::Id128));
    bool ok = true;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(QDir::toNativeSeparators(dbPath));
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READWRITE;QSQLITE_OPEN_CREATE"));
        ok = db.open();
        if (!ok) qWarning() << "Failed to open test Windsurf DB" << dbPath << db.lastError().text();

        if (ok) {
            QSqlQuery create(db);
            ok = create.exec(QStringLiteral("CREATE TABLE ItemTable(key TEXT PRIMARY KEY, value BLOB);"));
            if (!ok) qWarning() << "Failed to create test Windsurf DB schema" << create.lastError().text();
        }

        if (ok) {
            QSqlQuery insert(db);
            ok = insert.prepare(QStringLiteral("INSERT INTO ItemTable(key, value) VALUES(?, ?);"));
            if (!ok) qWarning() << "Failed to prepare test Windsurf insert" << insert.lastError().text();
            if (ok) {
                insert.addBindValue(QStringLiteral("windsurf.settings.cachedPlanInfo"));
                insert.addBindValue(jsonData);
                ok = insert.exec();
                if (!ok) qWarning() << "Failed to insert test Windsurf data" << insert.lastError().text();
            }
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName);
    return ok ? dbPath : QString();
}

QByteArray tst_WindsurfProvider::makePlanStatusResponse(const QString& planName,
                                                        int dailyRemaining,
                                                        int weeklyRemaining,
                                                        qint64 planEndUnix,
                                                        qint64 dailyResetUnix,
                                                        qint64 weeklyResetUnix)
{
    const QByteArray planInfo = stringField(2, planName);
    QByteArray planStatus;
    planStatus.append(messageField(1, planInfo));
    planStatus.append(messageField(3, timestamp(planEndUnix)));
    planStatus.append(varintField(14, static_cast<quint64>(dailyRemaining)));
    planStatus.append(varintField(15, static_cast<quint64>(weeklyRemaining)));
    planStatus.append(varintField(17, static_cast<quint64>(dailyResetUnix)));
    planStatus.append(varintField(18, static_cast<quint64>(weeklyResetUnix)));
    return messageField(1, planStatus);
}

QByteArray tst_WindsurfProvider::varint(quint64 value)
{
    QByteArray data;
    while (value >= 0x80) {
        data.append(static_cast<char>((value & 0x7f) | 0x80));
        value >>= 7;
    }
    data.append(static_cast<char>(value));
    return data;
}

QByteArray tst_WindsurfProvider::fieldKey(int number, int wireType)
{
    return varint(static_cast<quint64>((number << 3) | wireType));
}

QByteArray tst_WindsurfProvider::lengthDelimitedField(int number, const QByteArray& value)
{
    QByteArray data;
    data.append(fieldKey(number, 2));
    data.append(varint(static_cast<quint64>(value.size())));
    data.append(value);
    return data;
}

QByteArray tst_WindsurfProvider::stringField(int number, const QString& value)
{
    return lengthDelimitedField(number, value.toUtf8());
}

QByteArray tst_WindsurfProvider::varintField(int number, quint64 value)
{
    QByteArray data;
    data.append(fieldKey(number, 0));
    data.append(varint(value));
    return data;
}

QByteArray tst_WindsurfProvider::messageField(int number, const QByteArray& value)
{
    return lengthDelimitedField(number, value);
}

QByteArray tst_WindsurfProvider::timestamp(qint64 seconds)
{
    return varintField(1, static_cast<quint64>(seconds));
}

QTEST_MAIN(tst_WindsurfProvider)
#include "tst_WindsurfProvider.moc"
