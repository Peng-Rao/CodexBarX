#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class MiMoProvider : public IProvider {
    Q_OBJECT
public:
    explicit MiMoProvider(QObject* parent = nullptr);

    QString id() const override { return "mimo"; }
    QString displayName() const override { return "MiMo"; }
    QString sessionLabel() const override { return "Tokens"; }
    QString weeklyLabel() const override { return "Balance"; }
    bool supportsCredits() const override { return false; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"manualCookieHeader", "Cookie header", "secret", QVariant(),
             {}, "com.codexbarx.cookie.mimo", {}, "api-platform_serviceToken=...",
             "Session cookie from browser", true, true}
        };
    }

    QString brandColor() const override { return "#FF6900"; }
    QString dashboardURL() const override { return "https://platform.xiaomimimo.com/#/console/balance"; }
    QVector<QString> supportedSourceModes() const override { return {"web"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"cookie"}; }
};

class MiMoWebStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit MiMoWebStrategy(QObject* parent = nullptr);

    QString id() const override { return "mimo.web"; }
    int kind() const override { return ProviderFetchKind::Web; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& balanceJson,
                                              const QJsonObject& planJson,
                                              const QJsonObject& usageJson);
private:
    static QString resolveCookieHeader(const ProviderFetchContext& ctx);
};
