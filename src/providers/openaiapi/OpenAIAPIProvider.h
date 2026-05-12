#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class OpenAIAPIProvider : public IProvider {
    Q_OBJECT
public:
    explicit OpenAIAPIProvider(QObject* parent = nullptr);

    QString id() const override { return "openaiapi"; }
    QString displayName() const override { return "OpenAI API"; }
    QString sessionLabel() const override { return "API credits"; }
    QString weeklyLabel() const override { return "Spend"; }
    bool supportsCredits() const override { return true; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"apiKey", "API key", "secret", QVariant(),
             {}, "com.codexbarx.apikey.openaiapi", "OPENAI_API_KEY",
             "sk-...", "Stored in Windows Credential Manager", false, true}
        };
    }

    QString brandColor() const override { return "#0F826B"; }
    QString dashboardURL() const override {
        return "https://platform.openai.com/settings/organization/billing/overview";
    }
    QString statusLinkURL() const override { return "https://status.openai.com"; }
    QVector<QString> supportedSourceModes() const override { return {"api"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"apiKey"}; }
};

class OpenAIAPIAPIStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit OpenAIAPIAPIStrategy(QObject* parent = nullptr);

    QString id() const override { return "openaiapi.api"; }
    int kind() const override { return ProviderFetchKind::APIToken; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& json, int httpStatus = 200);
private:
    static QString resolveApiKey(const ProviderFetchContext& ctx);
};
