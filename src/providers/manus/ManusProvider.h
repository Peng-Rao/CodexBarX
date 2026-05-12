#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class ManusProvider : public IProvider {
    Q_OBJECT
public:
    explicit ManusProvider(QObject* parent = nullptr);

    QString id() const override { return "manus"; }
    QString displayName() const override { return "Manus"; }
    QString sessionLabel() const override { return "Credits"; }
    QString weeklyLabel() const override { return "Monthly"; }
    bool supportsCredits() const override { return true; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"manualCookieHeader", "Cookie header", "secret", QVariant(),
             {}, "com.codexbarx.cookie.manus", {}, "session_id=...",
             "Stored in Windows Credential Manager", true, true}
        };
    }

    QString brandColor() const override { return "#6366F1"; }
    QString dashboardURL() const override { return "https://manus.im"; }
    QVector<QString> supportedSourceModes() const override { return {"web"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"cookie"}; }
};

class ManusWebStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit ManusWebStrategy(QObject* parent = nullptr);

    QString id() const override { return "manus.web"; }
    int kind() const override { return ProviderFetchKind::Web; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& json);
private:
    static QString resolveCookieHeader(const ProviderFetchContext& ctx);
};
