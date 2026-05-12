#pragma once

#include "../IProvider.h"
#include "../IFetchStrategy.h"
#include "../ProviderFetchContext.h"
#include "../ProviderFetchResult.h"

#include <QObject>
#include <QString>
#include <QJsonObject>

class DoubaoProvider : public IProvider {
    Q_OBJECT
public:
    explicit DoubaoProvider(QObject* parent = nullptr);

    QString id() const override { return "doubao"; }
    QString displayName() const override { return "Doubao"; }
    QString sessionLabel() const override { return "Requests"; }
    QString weeklyLabel() const override { return "Rate limit"; }
    bool supportsCredits() const override { return false; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext& ctx) override;

    QVector<ProviderSettingsDescriptor> settingsDescriptors() const override {
        return {
            {"apiKey", "API key", "secret", QVariant(),
             {}, "com.codexbarx.apikey.doubao", "ARK_API_KEY",
             "ark-...", "Stored in Windows Credential Manager", false, true}
        };
    }

    QString brandColor() const override { return "#3370FF"; }
    QString dashboardURL() const override {
        return "https://console.volcengine.com/ark/region:ark+cn-beijing/openManagement";
    }
    QVector<QString> supportedSourceModes() const override { return {"api"}; }
    bool supportsMultipleAccounts() const override { return true; }
    QVector<QString> requiredCredentialTypes() const override { return {"apiKey"}; }
};

class DoubaoAPIStrategy : public IFetchStrategy {
    Q_OBJECT
public:
    explicit DoubaoAPIStrategy(QObject* parent = nullptr);

    QString id() const override { return "doubao.api"; }
    int kind() const override { return ProviderFetchKind::APIToken; }
    bool isAvailable(const ProviderFetchContext& ctx) const override;
    ProviderFetchResult fetchSync(const ProviderFetchContext& ctx) override;
    bool shouldFallback(const ProviderFetchResult& result, const ProviderFetchContext& ctx) const override;

    static ProviderFetchResult parseFromHeaders(const QHash<QString, QString>& headers, int httpStatus);
private:
    static QString resolveApiKey(const ProviderFetchContext& ctx);
    static const char* PROBE_MODELS[];
};
