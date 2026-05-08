#include "WindsurfDevinSessionImporter.h"
#include "../shared/BrowserDetection.h"
#include "../shared/ChromiumLocalStorageReader.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>

namespace {

const QStringList kTargetKeys = {
    QStringLiteral("devin_session_token"),
    QStringLiteral("devin_auth1_token"),
    QStringLiteral("devin_account_id"),
    QStringLiteral("devin_primary_org_id")
};

const QString kOrigin = QStringLiteral("https://windsurf.com");

} // namespace

WindsurfDevinSessionImporter::Overrides WindsurfDevinSessionImporter::s_overrides;

QVector<CookieImporter::Browser> WindsurfDevinSessionImporter::preferredBrowsers()
{
    return {CookieImporter::Chrome};
}

QVector<CookieImporter::Browser> WindsurfDevinSessionImporter::fallbackBrowsers()
{
    return {
        CookieImporter::Chrome,
        CookieImporter::Edge,
        CookieImporter::Brave,
        CookieImporter::Vivaldi,
        CookieImporter::Opera
    };
}

QVector<CookieImporter::Browser> WindsurfDevinSessionImporter::fallbackBrowsersExcludingPreferred()
{
    auto pref = preferredBrowsers();
    QSet<int> prefSet;
    for (auto b : pref) prefSet.insert(static_cast<int>(b));
    QVector<CookieImporter::Browser> result;
    for (auto b : fallbackBrowsers()) {
        if (!prefSet.contains(static_cast<int>(b))) {
            result.append(b);
        }
    }
    return result;
}

QString WindsurfDevinSessionImporter::decodedStorageValue(const QString& value)
{
    QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) return trimmed;

    while (!trimmed.isEmpty() && trimmed.front().unicode() == 0) {
        trimmed.remove(0, 1);
    }
    while (!trimmed.isEmpty() && trimmed.back().unicode() == 0) {
        trimmed.chop(1);
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError) {
        const QJsonValue val = doc.isArray() ? doc.array().at(0) : doc.object().value(QString());
        if (val.isString() && !val.toString().isEmpty()) {
            return val.toString().trimmed();
        }
    }

    if ((trimmed.startsWith('"') && trimmed.endsWith('"')) ||
        (trimmed.startsWith('\'') && trimmed.endsWith('\''))) {
        trimmed = trimmed.mid(1, trimmed.size() - 2).trimmed();
    }

    return trimmed;
}

std::optional<WindsurfDevinSessionInfo> WindsurfDevinSessionImporter::sessionFromStorage(
    const QHash<QString, QString>& storage,
    const QString& sourceLabel,
    const QString& levelDBPath)
{
    auto val = [&](const QString& key) -> QString {
        return storage.value(key).trimmed();
    };

    const QString sessionToken = val(kTargetKeys[0]);
    const QString auth1Token = val(kTargetKeys[1]);
    const QString accountID = val(kTargetKeys[2]);
    const QString primaryOrgID = val(kTargetKeys[3]);

    if (sessionToken.isEmpty() || auth1Token.isEmpty() ||
        accountID.isEmpty() || primaryOrgID.isEmpty()) {
        return std::nullopt;
    }

    WindsurfDevinSessionInfo info;
    info.session.sessionToken = sessionToken;
    info.session.auth1Token = auth1Token;
    info.session.accountID = accountID;
    info.session.primaryOrgID = primaryOrgID;
    info.sourceLabel = sourceLabel;
    info.levelDBPath = levelDBPath;
    return info;
}

QVector<WindsurfDevinSessionInfo> WindsurfDevinSessionImporter::deduplicateSessions(
    const QVector<WindsurfDevinSessionInfo>& sessions)
{
    QVector<WindsurfDevinSessionInfo> result;
    QSet<QString> seen;
    for (const auto& s : sessions) {
        const QString token = s.session.sessionToken.trimmed();
        if (token.isEmpty()) continue;
        if (seen.contains(token)) continue;
        seen.insert(token);
        result.append(s);
    }
    return result;
}

QVector<WindsurfDevinSessionInfo> WindsurfDevinSessionImporter::importSessionsForBrowser(
    CookieImporter::Browser browser,
    const QString& levelDBOverride)
{
    QVector<WindsurfDevinSessionInfo> sessions;

    const QString displayName = BrowserDetection::browserDisplayName(browser);
    QStringList profilePaths;

    if (!levelDBOverride.isEmpty()) {
        QDir dir(levelDBOverride);
        if (dir.exists()) {
            profilePaths.append(QDir::fromNativeSeparators(dir.absolutePath()));
        }
    }

    if (profilePaths.isEmpty()) {
        profilePaths = BrowserDetection::localStorageProfilePaths(browser);
    }

    for (const auto& profilePath : profilePaths) {
        const QString levelDBPath = profilePath;
        const bool isRootProfile = !profilePath.endsWith(QStringLiteral("/Default"))
            && !profilePath.contains(QStringLiteral("/Profile"))
            && !profilePath.contains(QStringLiteral("/Guest"))
            && !profilePath.contains(QStringLiteral("/user-"));

        QString profileName;
        if (!levelDBOverride.isEmpty()) {
            profileName = QStringLiteral("Windsurf LocalStorage Override");
        } else if (isRootProfile) {
            profileName = displayName;
        } else {
            const int lastSlash = profilePath.lastIndexOf('/');
            profileName = QStringLiteral("%1 %2")
                .arg(displayName)
                .arg(lastSlash >= 0 ? profilePath.mid(lastSlash + 1) : profilePath);
        }

        const QString sourceLabel = profileName;

        QString levelDBDir = levelDBPath;
        if (!levelDBDir.endsWith(QStringLiteral("/leveldb"))) {
            levelDBDir += QStringLiteral("/Local Storage/leveldb");
        }

        if (!QFileInfo::exists(levelDBDir)) continue;

        QHash<QString, QString> storage;

        auto entries = ChromiumLocalStorageReader::readEntries(kOrigin, levelDBDir, kTargetKeys);
        for (const auto& entry : entries) {
            storage[entry.key] = decodedStorageValue(entry.value);
        }

        if (storage.size() < kTargetKeys.size()) {
            auto textEntries = ChromiumLocalStorageReader::readTextEntries(levelDBDir, kTargetKeys);
            for (const auto& entry : textEntries) {
                if (!storage.contains(entry.key)) {
                    storage[entry.key] = decodedStorageValue(entry.value);
                }
            }
        }

        auto session = sessionFromStorage(storage, sourceLabel, levelDBDir);
        if (session.has_value()) {
            sessions.append(*session);
        }
    }

    return deduplicateSessions(sessions);
}

QVector<WindsurfDevinSessionInfo> WindsurfDevinSessionImporter::importSessions(
    const ProviderFetchContext& ctx)
{
    if (s_overrides.importSessions) {
        return s_overrides.importSessions(ctx);
    }

    const QString overrideDir = qEnvironmentVariable("CODEXBAR_WINDSURF_LOCAL_STORAGE_DIR").trimmed();

    QVector<WindsurfDevinSessionInfo> sessions;
    for (auto browser : preferredBrowsers()) {
        sessions.append(importSessionsForBrowser(browser, overrideDir));
    }

    if (sessions.isEmpty()) {
        for (auto browser : fallbackBrowsersExcludingPreferred()) {
            sessions.append(importSessionsForBrowser(browser, overrideDir));
        }
    }

    return sessions;
}

QVector<WindsurfDevinSessionInfo> WindsurfDevinSessionImporter::importPreferredSessions(
    const ProviderFetchContext& ctx)
{
    if (s_overrides.importPreferredSessions) {
        return s_overrides.importPreferredSessions(ctx);
    }
    const QString overrideDir = qEnvironmentVariable("CODEXBAR_WINDSURF_LOCAL_STORAGE_DIR").trimmed();
    QVector<WindsurfDevinSessionInfo> sessions;
    for (auto browser : preferredBrowsers()) {
        sessions.append(importSessionsForBrowser(browser, overrideDir));
    }
    return sessions;
}

QVector<WindsurfDevinSessionInfo> WindsurfDevinSessionImporter::importFallbackSessions(
    const ProviderFetchContext& ctx)
{
    if (s_overrides.importFallbackSessions) {
        return s_overrides.importFallbackSessions(ctx);
    }
    const QString overrideDir = qEnvironmentVariable("CODEXBAR_WINDSURF_LOCAL_STORAGE_DIR").trimmed();
    QVector<WindsurfDevinSessionInfo> sessions;
    for (auto browser : fallbackBrowsersExcludingPreferred()) {
        sessions.append(importSessionsForBrowser(browser, overrideDir));
    }
    return sessions;
}

void WindsurfDevinSessionImporter::setImportSessionsOverride(WindsurfSessionImportFn fn)
{
    s_overrides.importSessions = std::move(fn);
}

void WindsurfDevinSessionImporter::setImportPreferredSessionsOverride(WindsurfSessionImportFn fn)
{
    s_overrides.importPreferredSessions = std::move(fn);
}

void WindsurfDevinSessionImporter::setImportFallbackSessionsOverride(WindsurfSessionImportFn fn)
{
    s_overrides.importFallbackSessions = std::move(fn);
}

void WindsurfDevinSessionImporter::clearOverrides()
{
    s_overrides = {};
}
