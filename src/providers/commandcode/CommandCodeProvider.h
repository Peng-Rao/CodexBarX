#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class CommandCodeProvider : public IProvider {
    Q_OBJECT
public:
    explicit CommandCodeProvider(QObject* parent = nullptr);

    QString id() const override { return "commandcode"; }
    QString displayName() const override { return "Command Code"; }
    QString sessionLabel() const override { return "Monthly credits"; }
    QString weeklyLabel() const override { return "Monthly"; }
    bool supportsCredits() const override { return true; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"manualCookieHeader", "Cookie header", "secret", QVariant(),
             {}, "com.codexbarx.cookie.commandcode", {}, "Cookie: ...",
             "Session cookie from browser", true, true}
        };
    }

    QString brandColor() const override { return "#000000"; }
    QString dashboardURL() const override { return "https://commandcode.ai/studio"; }
    QVector<QString> supportedSourceModes() const override { return {"web"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"cookie"}; }
};

class CommandCodeWebStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit CommandCodeWebStrategy(QObject* parent = nullptr);

    QString id() const override { return "commandcode.web"; }
    int kind() const override { return ProviderFetchKind::Web; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& creditsJson, const QJsonObject& subJson);
private:
    static QString resolveCookieHeader(const ProviderFetchContext& ctx);
};
