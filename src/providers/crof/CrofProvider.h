#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class CrofProvider : public IProvider {
    Q_OBJECT
public:
    explicit CrofProvider(QObject* parent = nullptr);

    QString id() const override { return "crof"; }
    QString displayName() const override { return "Crof"; }
    QString sessionLabel() const override { return "Requests"; }
    QString weeklyLabel() const override { return "Credits"; }
    bool supportsCredits() const override { return false; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"apiKey", "API key", "secret", QVariant(),
             {}, "com.codexbarx.apikey.crof", "CROF_API_KEY",
             "crof_...", "Stored in Windows Credential Manager", false, true}
        };
    }

    QString brandColor() const override { return "#2EAB96"; }
    QString dashboardURL() const override { return "https://crof.ai/dashboard"; }
    QVector<QString> supportedSourceModes() const override { return {"api"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"apiKey"}; }
};

class CrofAPIStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit CrofAPIStrategy(QObject* parent = nullptr);

    QString id() const override { return "crof.api"; }
    int kind() const override { return ProviderFetchKind::APIToken; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseResponse(const QJsonObject& json);
private:
    static QString resolveApiKey(const ProviderFetchContext& ctx);
};
