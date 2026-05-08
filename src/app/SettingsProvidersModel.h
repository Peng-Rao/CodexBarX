#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QPointer>
#include <QVariantList>
#include <QVariantMap>

class UsageStore;

class SettingsProviderListModel : public QAbstractListModel {
public:
    enum Role {
        ProviderIdRole = Qt::UserRole + 1,
        NameRole,
        EnabledRole,
        BrandColorRole,
        UsageRole,
        StatusRole,
        LastUpdatedRole,
    };

    explicit SettingsProviderListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setProviders(const QVariantList& providers);
    QString providerIdAt(int row) const;

private:
    QVariantList m_providers;
};

class SettingsProvidersModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QAbstractListModel* providers READ providers CONSTANT)
    Q_PROPERTY(int providerCount READ providerCount NOTIFY providersChanged)
    Q_PROPERTY(QString selectedProvider READ selectedProvider NOTIFY selectedProviderChanged)
    Q_PROPERTY(QVariantMap selectedDescriptor READ selectedDescriptor NOTIFY selectedDescriptorChanged)
    Q_PROPERTY(QString detailState READ detailState NOTIFY detailStateChanged)
    Q_PROPERTY(QVariantMap selectedConnectionTest READ selectedConnectionTest NOTIFY selectedConnectionTestChanged)
    Q_PROPERTY(QVariantMap selectedProviderStatus READ selectedProviderStatus NOTIFY selectedProviderStatusChanged)
    Q_PROPERTY(QString selectedProviderError READ selectedProviderError NOTIFY selectedProviderErrorChanged)
    Q_PROPERTY(QVariantMap selectedUsageSnapshot READ selectedUsageSnapshot NOTIFY selectedUsageSnapshotChanged)
    Q_PROPERTY(QVariantList selectedTokenAccounts READ selectedTokenAccounts NOTIFY selectedTokenAccountsChanged)
    Q_PROPERTY(QString selectedDefaultTokenAccountId READ selectedDefaultTokenAccountId NOTIFY selectedTokenAccountsChanged)
    Q_PROPERTY(QVariantMap tokenAccountOperationState READ tokenAccountOperationState NOTIFY tokenAccountOperationStateChanged)
    Q_PROPERTY(QVariantMap codexAccountState READ codexAccountState NOTIFY codexAccountStateChanged)
    Q_PROPERTY(QVariantMap codexProjection READ codexProjection NOTIFY codexProjectionChanged)

public:
    explicit SettingsProvidersModel(UsageStore* store, QObject* parent = nullptr);

    QAbstractListModel* providers() { return &m_providers; }
    int providerCount() const { return m_providerCount; }
    QString selectedProvider() const { return m_selectedProvider; }
    QVariantMap selectedDescriptor() const { return m_selectedDescriptor; }
    QString detailState() const { return m_detailState; }
    QVariantMap selectedConnectionTest() const { return m_selectedConnectionTest; }
    QVariantMap selectedProviderStatus() const { return m_selectedProviderStatus; }
    QString selectedProviderError() const { return m_selectedProviderError; }
    QVariantMap selectedUsageSnapshot() const { return m_selectedUsageSnapshot; }
    QVariantList selectedTokenAccounts() const { return m_selectedTokenAccounts; }
    QString selectedDefaultTokenAccountId() const { return m_selectedDefaultTokenAccountId; }
    QVariantMap tokenAccountOperationState() const { return m_tokenAccountOperationState; }
    QVariantMap codexAccountState() const { return m_codexAccountState; }
    QVariantMap codexProjection() const { return m_codexProjection; }

    Q_INVOKABLE void requestOpenProvidersTab();
    Q_INVOKABLE void selectProvider(const QString& providerId);
    Q_INVOKABLE void moveProvider(int fromIndex, int toIndex);
    Q_INVOKABLE void setProviderEnabled(const QString& providerId, bool enabled);
    Q_INVOKABLE void testConnection(const QString& providerId);
    Q_INVOKABLE void refreshProvider(const QString& providerId);
    Q_INVOKABLE void setProviderSetting(const QString& providerId, const QString& key, const QVariant& value);
    Q_INVOKABLE void setProviderSecret(const QString& providerId, const QString& key, const QString& value);
    Q_INVOKABLE void clearProviderSecret(const QString& providerId, const QString& key);

    Q_INVOKABLE void requestAddTokenAccount(const QString& providerId, const QString& displayName, int sourceMode);
    Q_INVOKABLE void requestAddTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey);
    Q_INVOKABLE void requestRemoveTokenAccount(const QString& accountId);
    Q_INVOKABLE void requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId);
    Q_INVOKABLE void requestSetTokenAccountSourceMode(const QString& accountId, int sourceMode);
    Q_INVOKABLE void requestSetTokenAccountVisibility(const QString& accountId, int visibility);

    Q_INVOKABLE void setCodexActiveAccount(const QString& accountId);
    Q_INVOKABLE void addCodexAccount();
    Q_INVOKABLE void cancelCodexAuthentication();
    Q_INVOKABLE void removeCodexAccount(const QString& accountId);
    Q_INVOKABLE void reauthenticateCodexAccount(const QString& accountId);
    Q_INVOKABLE void promoteCodexAccount(const QString& accountId);

signals:
    void providersChanged();
    void selectedProviderChanged();
    void selectedDescriptorChanged();
    void detailStateChanged();
    void selectedConnectionTestChanged();
    void selectedProviderStatusChanged();
    void selectedProviderErrorChanged();
    void selectedUsageSnapshotChanged();
    void selectedTokenAccountsChanged();
    void tokenAccountOperationStateChanged();
    void codexAccountStateChanged();
    void codexProjectionChanged();

private:
    void syncProviderList();
    void requestSelectedDescriptor();
    void syncSelectedDescriptor();
    void syncSelectedConnectionTest();
    void syncSelectedStatus();
    void syncSelectedError();
    void syncSelectedUsageSnapshot();
    void syncSelectedTokenAccounts();
    void syncTokenOperationState();
    void syncCodexState();
    void syncCodexProjection();
    void setDetailState(const QString& state);
    void selectFirstProviderIfNeeded();

    QPointer<UsageStore> m_store;
    SettingsProviderListModel m_providers;
    int m_providerCount = 0;
    QString m_selectedProvider;
    QVariantMap m_selectedDescriptor;
    QString m_detailState = QStringLiteral("idle");
    QVariantMap m_selectedConnectionTest;
    QVariantMap m_selectedProviderStatus;
    QString m_selectedProviderError;
    QVariantMap m_selectedUsageSnapshot;
    QVariantList m_selectedTokenAccounts;
    QString m_selectedDefaultTokenAccountId;
    QVariantMap m_tokenAccountOperationState;
    QVariantMap m_codexAccountState;
    QVariantMap m_codexProjection;
};
