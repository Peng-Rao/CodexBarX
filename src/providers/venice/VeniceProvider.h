#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class VeniceProvider : public IProvider {
    Q_OBJECT
public:
    explicit VeniceProvider(QObject* parent = nullptr);

    QString id() const override { return "venice"; }
    QString displayName() const override { return "Venice"; }
    QString sessionLabel() const override { return "Balance"; }
    QString weeklyLabel() const override { return "Balance"; }
    bool supportsCredits() const override { return false; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"apiKey", "API key", "secret", QVariant(),
             {}, "com.codexbarx.apikey.venice", "VENICE_API_KEY",
             "venice_...", "Stored in Windows Credential Manager", false, true}
        };
    }

    QString brandColor() const override { return "#3399FF"; }
    QString dashboardURL() const override { return "https://venice.ai/settings/api"; }
    QVector<QString> supportedSourceModes() const override { return {"api"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"apiKey"}; }
};

class VeniceAPIStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit VeniceAPIStrategy(QObject* parent = nullptr);

    QString id() const override { return "venice.api"; }
    int kind() const override { return ProviderFetchKind::APIToken; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& json);
private:
    static QString resolveApiKey(const ProviderFetchContext& ctx);
};
