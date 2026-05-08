#pragma once

#include "WindsurfProvider.h"
#include "../shared/CookieImporter.h"
#include "../ProviderFetchContext.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <optional>

struct WindsurfDevinSessionInfo {
    WindsurfDevinSessionAuth session;
    QString sourceLabel;
    QString levelDBPath;
};

using WindsurfSessionImportFn = std::function<QVector<WindsurfDevinSessionInfo>(
    const ProviderFetchContext&)>;

class WindsurfDevinSessionImporter {
public:
    static QVector<CookieImporter::Browser> preferredBrowsers();
    static QVector<CookieImporter::Browser> fallbackBrowsers();
    static QVector<CookieImporter::Browser> fallbackBrowsersExcludingPreferred();

    static QVector<WindsurfDevinSessionInfo> importPreferredSessions(
        const ProviderFetchContext& ctx);
    static QVector<WindsurfDevinSessionInfo> importFallbackSessions(
        const ProviderFetchContext& ctx);
    static QVector<WindsurfDevinSessionInfo> importSessions(
        const ProviderFetchContext& ctx);

    static std::optional<WindsurfDevinSessionInfo> sessionFromStorage(
        const QHash<QString, QString>& storage,
        const QString& sourceLabel,
        const QString& levelDBPath = {});

    static QString decodedStorageValue(const QString& value);
    static QVector<WindsurfDevinSessionInfo> deduplicateSessions(
        const QVector<WindsurfDevinSessionInfo>& sessions);

    static void setImportSessionsOverride(WindsurfSessionImportFn fn);
    static void setImportPreferredSessionsOverride(WindsurfSessionImportFn fn);
    static void setImportFallbackSessionsOverride(WindsurfSessionImportFn fn);
    static void clearOverrides();

private:
    static QVector<WindsurfDevinSessionInfo> importSessionsForBrowser(
        CookieImporter::Browser browser,
        const QString& levelDBOverride);

    struct Overrides {
        WindsurfSessionImportFn importSessions;
        WindsurfSessionImportFn importPreferredSessions;
        WindsurfSessionImportFn importFallbackSessions;
    };
    static Overrides s_overrides;
};
