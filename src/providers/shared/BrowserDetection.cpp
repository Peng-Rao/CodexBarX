#include "BrowserDetection.h"

#include <QDir>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace {

QString envPath(const QString& key) {
    return QDir::fromNativeSeparators(qEnvironmentVariable(key.toUtf8().constData())).trimmed();
}

QString localAppData() {
    QString v = envPath("LOCALAPPDATA");
    if (!v.isEmpty()) return v;
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
}

QString roamingAppData() {
    QString v = envPath("APPDATA");
    if (!v.isEmpty()) return v;
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString chromiumRoot(CookieImporter::Browser browser) {
    switch (browser) {
    case CookieImporter::Chrome: {
        QString override = envPath("CODEXBAR_CHROME_USER_DATA_DIR");
        return override.isEmpty() ? localAppData() + "/Google/Chrome/User Data" : override;
    }
    case CookieImporter::Edge: {
        QString override = envPath("CODEXBAR_EDGE_USER_DATA_DIR");
        return override.isEmpty() ? localAppData() + "/Microsoft/Edge/User Data" : override;
    }
    case CookieImporter::Brave: {
        QString override = envPath("CODEXBAR_BRAVE_USER_DATA_DIR");
        return override.isEmpty() ? localAppData() + "/BraveSoftware/Brave-Browser/User Data" : override;
    }
    case CookieImporter::Opera: {
        QString override = envPath("CODEXBAR_OPERA_USER_DATA_DIR");
        return override.isEmpty() ? roamingAppData() + "/Opera Software/Opera Stable" : override;
    }
    case CookieImporter::Vivaldi: {
        QString override = envPath("CODEXBAR_VIVALDI_USER_DATA_DIR");
        return override.isEmpty() ? localAppData() + "/Vivaldi/User Data" : override;
    }
    case CookieImporter::Firefox:
        return {};
    }
    return {};
}

QString firefoxProfilesRoot() {
    QString override = envPath("CODEXBAR_FIREFOX_PROFILES_DIR");
    return override.isEmpty() ? roamingAppData() + "/Mozilla/Firefox/Profiles" : override;
}

QString chromiumCookiePathForProfile(const QString& profile) {
    QString network = profile + "/Network/Cookies";
    if (QFileInfo::exists(network)) return network;
    QString legacy = profile + "/Cookies";
    if (QFileInfo::exists(legacy)) return legacy;
    return network;
}

bool hasChromiumCookieStore(const QString& profile) {
    return QFileInfo::exists(profile + "/Network/Cookies") || QFileInfo::exists(profile + "/Cookies");
}

} // namespace

QVector<CookieImporter::Browser> BrowserDetection::installedBrowsers() {
    QVector<CookieImporter::Browser> result;
    for (auto b : CookieImporter::importOrder()) {
        if (CookieImporter::isBrowserInstalled(b)) {
            result.append(b);
        }
    }
    return result;
}

QString BrowserDetection::cookiePath(CookieImporter::Browser browser) {
    auto profiles = profilePaths(browser);
    if (profiles.isEmpty()) return {};
    if (browser == CookieImporter::Firefox) {
        return profiles.first() + "/cookies.sqlite";
    }
    return chromiumCookiePathForProfile(profiles.first());
}

QString BrowserDetection::localStatePath(CookieImporter::Browser browser) {
    if (browser == CookieImporter::Firefox) return {};
    QString root = chromiumRoot(browser);
    if (root.isEmpty()) return {};
    return root + "/Local State";
}

QStringList BrowserDetection::profilePaths(CookieImporter::Browser browser) {
    QStringList profiles;

    if (browser == CookieImporter::Firefox) {
        QDir root(firefoxProfilesRoot());
        if (!root.exists()) return {};
        auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& info : entries) {
            QString path = QDir::fromNativeSeparators(info.absoluteFilePath());
            if (QFileInfo::exists(path + "/cookies.sqlite")) {
                profiles.append(path);
            }
        }
        return profiles;
    }

    QString rootPath = chromiumRoot(browser);
    if (rootPath.isEmpty()) return {};
    QDir root(rootPath);
    if (!root.exists()) return {};

    if (hasChromiumCookieStore(rootPath)) {
        profiles.append(rootPath);
    }

    QString defaultPath = rootPath + "/Default";
    if (hasChromiumCookieStore(defaultPath)) {
        profiles.append(defaultPath);
    }

    auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& info : entries) {
        QString name = info.fileName();
        if (!name.startsWith("Profile", Qt::CaseInsensitive)) continue;
        QString path = QDir::fromNativeSeparators(info.absoluteFilePath());
        if (hasChromiumCookieStore(path) && !profiles.contains(path)) {
            profiles.append(path);
        }
    }

    return profiles;
}

QString BrowserDetection::chromiumUserDataRoot(CookieImporter::Browser browser)
{
    return chromiumRoot(browser);
}

QString BrowserDetection::browserDisplayName(CookieImporter::Browser browser)
{
    switch (browser) {
    case CookieImporter::Chrome: return QStringLiteral("Chrome");
    case CookieImporter::Edge: return QStringLiteral("Edge");
    case CookieImporter::Firefox: return QStringLiteral("Firefox");
    case CookieImporter::Brave: return QStringLiteral("Brave");
    case CookieImporter::Opera: return QStringLiteral("Opera");
    case CookieImporter::Vivaldi: return QStringLiteral("Vivaldi");
    }
    return {};
}

static bool hasLocalStorageDir(const QString& profile)
{
    return QFileInfo::exists(profile + QStringLiteral("/Local Storage/leveldb"));
}

static QStringList chromiumLocalStorageProfiles(const QString& rootPath)
{
    QStringList profiles;
    QDir root(rootPath);
    if (!root.exists()) return profiles;

    if (hasLocalStorageDir(rootPath)) {
        profiles.append(rootPath);
    }

    if (hasLocalStorageDir(rootPath + QStringLiteral("/Default"))) {
        profiles.append(rootPath + QStringLiteral("/Default"));
    }

    const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    QStringList unnumbered;
    for (const auto& info : entries) {
        const QString name = info.fileName();
        const QString path = QDir::fromNativeSeparators(info.absoluteFilePath());
        if (name.startsWith(QStringLiteral("Profile"), Qt::CaseInsensitive)) {
            if (hasLocalStorageDir(path) && !profiles.contains(path)) {
                profiles.append(path);
            }
        } else if (name.startsWith(QStringLiteral("user-"), Qt::CaseInsensitive)) {
            if (hasLocalStorageDir(path) && !profiles.contains(path)) {
                unnumbered.append(path);
            }
        }
    }

    unnumbered.sort();
    for (const auto& p : unnumbered) {
        profiles.append(p);
    }

    if (hasLocalStorageDir(rootPath + QStringLiteral("/Guest Profile"))) {
        const QString guest = rootPath + QStringLiteral("/Guest Profile");
        if (!profiles.contains(guest)) {
            profiles.append(guest);
        }
    }

    return profiles;
}

QStringList BrowserDetection::localStorageProfilePaths(CookieImporter::Browser browser)
{
    if (browser == CookieImporter::Firefox) return {};
    const QString rootPath = chromiumRoot(browser);
    if (rootPath.isEmpty()) return {};
    return chromiumLocalStorageProfiles(rootPath);
}
