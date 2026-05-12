#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class StepFunProvider : public IProvider {
    Q_OBJECT
public:
    explicit StepFunProvider(QObject* parent = nullptr);

    QString id() const override { return "stepfun"; }
    QString displayName() const override { return "StepFun"; }
    QString sessionLabel() const override { return "5h quota"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool supportsCredits() const override { return false; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"authMode", "Auth mode", "picker", QVariant("token"),
             { {"token", "Token"}, {"login", "Username + Password"} }},
            {"username", "Username", "text", QVariant(),
             {}, {}, "STEPFUN_USERNAME", "user@example.com", "Phone or email"},
            {"password", "Password", "secret", QVariant(),
             {}, "com.codexbarx.creds.stepfun", "STEPFUN_PASSWORD",
             "Password", "Used to obtain session token"},
            {"token", "Oasis-Token", "secret", QVariant(),
             {}, "com.codexbarx.token.stepfun", "STEPFUN_TOKEN",
             "Oasis-Token=...", "Manual token entry", false, true}
        };
    }

    QString brandColor() const override { return "#7C3AED"; }
    QString dashboardURL() const override { return "https://platform.stepfun.com/plan-usage"; }
    QVector<QString> supportedSourceModes() const override { return {"web"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"token"}; }
};

class StepFunWebStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit StepFunWebStrategy(QObject* parent = nullptr);

    QString id() const override { return "stepfun.web"; }
    int kind() const override { return ProviderFetchKind::Web; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& json);
private:
    static QString resolveToken(const ProviderFetchContext& ctx);
};
