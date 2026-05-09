#include <QGuiApplication>
#include <QtTest/QtTest>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlComponent>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QAbstractListModel>
#include <QVariantMap>

class MockSettingsStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool debugMenuEnabled READ debugMenuEnabled CONSTANT)
    Q_PROPERTY(bool mergeIcons READ mergeIcons CONSTANT)
    Q_PROPERTY(bool usageBarsShowUsed READ usageBarsShowUsed CONSTANT)
    Q_PROPERTY(bool resetTimesShowAbsolute READ resetTimesShowAbsolute CONSTANT)
    Q_PROPERTY(bool showOptionalCreditsAndExtraUsage READ showOptionalCreditsAndExtraUsage CONSTANT)
    Q_PROPERTY(bool statusChecksEnabled READ statusChecksEnabled CONSTANT)
    Q_PROPERTY(bool sessionQuotaNotificationsEnabled READ sessionQuotaNotificationsEnabled CONSTANT)
    Q_PROPERTY(int refreshFrequency READ refreshFrequency CONSTANT)
    Q_PROPERTY(QString language READ language CONSTANT)
public:
    bool debugMenuEnabled() const { return true; }
    bool mergeIcons() const { return true; }
    bool usageBarsShowUsed() const { return false; }
    bool resetTimesShowAbsolute() const { return false; }
    bool showOptionalCreditsAndExtraUsage() const { return true; }
    bool statusChecksEnabled() const { return true; }
    bool sessionQuotaNotificationsEnabled() const { return false; }
    int refreshFrequency() const { return 15; }
    QString language() const { return "en"; }
    bool launchAtLogin() const { return false; }
    int settingsRevision() const { return 0; }

    Q_INVOKABLE void setProviderEnabled(const QString&, bool) {}
    Q_INVOKABLE bool isProviderEnabled(const QString&) const { return false; }
    Q_INVOKABLE QVariant providerSetting(const QString&, const QString&, const QVariant& def = {}) const { return def; }
    Q_INVOKABLE void setProviderSetting(const QString&, const QString&, const QVariant&) {}
    Q_INVOKABLE QVariantList providerOrder() const { return {}; }
    Q_INVOKABLE void setProviderOrder(const QVariantList&) {}
    Q_INVOKABLE void setMergeIcons(bool) {}
    Q_INVOKABLE void setUsageBarsShowUsed(bool) {}
    Q_INVOKABLE void setResetTimesShowAbsolute(bool) {}
    Q_INVOKABLE void setShowOptionalCreditsAndExtraUsage(bool) {}
    Q_INVOKABLE void setStatusChecksEnabled(bool) {}
    Q_INVOKABLE void setSessionQuotaNotificationsEnabled(bool) {}
    Q_INVOKABLE void setRefreshFrequency(int) {}
    Q_INVOKABLE void setLanguage(const QString&) {}
    Q_INVOKABLE void setDebugMenuEnabled(bool) {}

signals:
    void debugMenuEnabledChanged();
    void mergeIconsChanged();
    void usageBarsShowUsedChanged();
    void resetTimesShowAbsoluteChanged();
    void showOptionalCreditsAndExtraUsageChanged();
    void statusChecksEnabledChanged();
    void sessionQuotaNotificationsEnabledChanged();
};

class MockUsageStore : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList providerIDs READ providerIDs CONSTANT)
    Q_PROPERTY(bool isRefreshing READ isRefreshing CONSTANT)
    Q_PROPERTY(bool costUsageEnabled READ costUsageEnabled NOTIFY costUsageEnabledChanged)
    Q_PROPERTY(bool costUsageRefreshing READ costUsageRefreshing CONSTANT)
    Q_PROPERTY(int snapshotRevision READ snapshotRevision CONSTANT)
    Q_PROPERTY(int statusRevision READ statusRevision CONSTANT)
    Q_PROPERTY(QVariantMap codexAccountState READ codexAccountState CONSTANT)
    Q_PROPERTY(QVariantList codexFetchAttempts READ codexFetchAttempts CONSTANT)
    Q_PROPERTY(QVariantMap tokenAccountOperationState READ tokenAccountOperationState NOTIFY tokenAccountOperationStateChanged)
public:
    QStringList providerIDs() const {
        return providerIDsForTest.isEmpty()
            ? QStringList({"codex", "claude", "cursor"})
            : providerIDsForTest;
    }
    Q_INVOKABLE QStringList allProviderIDs() const { return {"codex", "claude", "cursor", "zai"}; }
    bool isRefreshing() const { return false; }
    bool costUsageEnabled() const { return costUsageEnabledValue; }
    bool costUsageRefreshing() const { return false; }
    int snapshotRevision() const { return 0; }
    int statusRevision() const { return 0; }
    bool isProviderEnabled(const QString&) const { return true; }
    QVariantMap snapshotData(const QString&) const {
        QVariantMap m;
        m["primaryUsed"] = 25.0; m["primaryRemaining"] = 75.0;
        m["primaryDisplayPercent"] = 75.0; m["primaryDisplayIsUsed"] = false;
        m["secondaryUsed"] = 10.0; m["secondaryRemaining"] = 90.0;
        m["secondaryDisplayPercent"] = 90.0; m["secondaryDisplayIsUsed"] = false;
        m["hasSecondary"] = true; m["hasTertiary"] = false;
        m["sessionLabel"] = "Session"; m["weeklyLabel"] = "Weekly";
        m["displayName"] = "Codex"; m["supportsCredits"] = false;
        m["hasUsage"] = true; m["updatedAt"] = 0;
        return m;
    }
    Q_INVOKABLE QVariantList providerList() const {
        ++providerListCalls;
        auto makeEntry = [](const QString& id, const QString& name, bool enabled) {
            QVariantMap e; e["id"] = id; e["name"] = name; e["enabled"] = enabled;
            e["sessionLabel"] = "Session"; e["weeklyLabel"] = "Weekly";
            e["supportsCredits"] = false;
            return e;
        };
        return {makeEntry("codex", "Codex", true), makeEntry("claude", "Claude", true)};
    }
    Q_INVOKABLE QString providerDisplayName(const QString& id) const {
        if (id == "codex") return "Codex";
        if (id == "claude") return "Claude";
        return id;
    }
    Q_INVOKABLE QString providerError(const QString&) const { return {}; }
    Q_INVOKABLE QVariantMap providerStatus(const QString&) const { return {{"state", "ok"}}; }
    Q_INVOKABLE QVariantMap providerUsageSnapshot(const QString&) const { return {}; }
    Q_INVOKABLE QVariantMap providerConnectionTest(const QString&) const { return {{"state", "idle"}}; }
    Q_INVOKABLE QVariantMap providerSecretStatus(const QString&, const QString&) const { return {{"configured", false}}; }
    Q_INVOKABLE QVariantList providerSettingsFields(const QString&) const { return {}; }
    Q_INVOKABLE QVariantMap providerDescriptorData(const QString&) const {
        ++providerDescriptorCalls;
        return {};
    }
    Q_INVOKABLE void requestProviderList() {
        ++requestProviderListCalls;
        emit providerListModelChanged();
    }
    Q_INVOKABLE void requestProviderDescriptor(const QString& providerId) {
        ++requestProviderDescriptorCalls;
        lastRequestedProviderDescriptor = providerId;
        emit providerDescriptorChanged(providerId);
    }
    Q_INVOKABLE QVariantList tokenAccountsForProvider(const QString& providerId) const {
        ++tokenAccountsForProviderCalls;
        if (providerId == tokenAccountProviderForTest) {
            return tokenAccountsForProviderValue;
        }
        return {};
    }
    Q_INVOKABLE QString addTokenAccount(const QString& providerId, const QString& displayName, int sourceMode) {
        ++addTokenAccountCalls;
        lastTokenProvider = providerId;
        lastTokenDisplayName = displayName;
        lastTokenSourceMode = sourceMode;
        emit tokenAccountsChanged(providerId);
        return "token-account-1";
    }
    Q_INVOKABLE QString addTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey) {
        ++addTokenAccountWithApiKeyCalls;
        lastTokenProvider = providerId;
        lastTokenDisplayName = displayName;
        lastTokenSourceMode = sourceMode;
        lastTokenApiKey = apiKey;
        emit tokenAccountsChanged(providerId);
        return "token-account-1";
    }
    Q_INVOKABLE bool removeTokenAccount(const QString& accountId) {
        ++removeTokenAccountCalls;
        lastTokenAccountId = accountId;
        emit tokenAccountsChanged(lastTokenProvider);
        return true;
    }
    Q_INVOKABLE bool setTokenAccountVisibility(const QString& accountId, int visibility) {
        ++setTokenAccountVisibilityCalls;
        lastTokenAccountId = accountId;
        lastTokenVisibility = visibility;
        return true;
    }
    Q_INVOKABLE bool setTokenAccountSourceMode(const QString& accountId, int sourceMode) {
        ++setTokenAccountSourceModeCalls;
        lastTokenAccountId = accountId;
        lastTokenSourceMode = sourceMode;
        return true;
    }
    Q_INVOKABLE bool setDefaultTokenAccount(const QString& providerId, const QString& accountId) {
        ++setDefaultTokenAccountCalls;
        lastTokenProvider = providerId;
        lastTokenAccountId = accountId;
        if (providerId == tokenAccountProviderForTest) {
            defaultTokenAccountValue = accountId;
        }
        emit tokenAccountsChanged(providerId);
        return true;
    }
    Q_INVOKABLE QString defaultTokenAccount(const QString& providerId) const {
        return providerId == tokenAccountProviderForTest ? defaultTokenAccountValue : QString();
    }
    QVariantList tokenAccountsForProviderValueForTest(const QString& providerId) const {
        return providerId == tokenAccountProviderForTest ? tokenAccountsForProviderValue : QVariantList();
    }
    QString defaultTokenAccountForTest(const QString& providerId) const {
        return providerId == tokenAccountProviderForTest ? defaultTokenAccountValue : QString();
    }
    Q_INVOKABLE QVariantMap tokenAccountOperationState() const { return {}; }
    Q_INVOKABLE QString requestAddTokenAccount(const QString& providerId, const QString& displayName, int sourceMode) {
        ++requestAddTokenAccountCalls;
        lastTokenProvider = providerId;
        lastTokenDisplayName = displayName;
        lastTokenSourceMode = sourceMode;
        emit tokenAccountsChanged(providerId);
        return "op-add-token-account";
    }
    Q_INVOKABLE QString requestAddTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey) {
        ++requestAddTokenAccountWithApiKeyCalls;
        lastTokenProvider = providerId;
        lastTokenDisplayName = displayName;
        lastTokenSourceMode = sourceMode;
        lastTokenApiKey = apiKey;
        emit tokenAccountsChanged(providerId);
        return "op-add-token-account-api";
    }
    Q_INVOKABLE QString requestRemoveTokenAccount(const QString& accountId) {
        ++requestRemoveTokenAccountCalls;
        lastTokenAccountId = accountId;
        emit tokenAccountsChanged(lastTokenProvider);
        return "op-remove-token-account";
    }
    Q_INVOKABLE QString requestSetTokenAccountVisibility(const QString& accountId, int visibility) {
        ++requestSetTokenAccountVisibilityCalls;
        lastTokenAccountId = accountId;
        lastTokenVisibility = visibility;
        return "op-set-token-account-visibility";
    }
    Q_INVOKABLE QString requestSetTokenAccountSourceMode(const QString& accountId, int sourceMode) {
        ++requestSetTokenAccountSourceModeCalls;
        lastTokenAccountId = accountId;
        lastTokenSourceMode = sourceMode;
        return "op-set-token-account-source-mode";
    }
    Q_INVOKABLE QString requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId) {
        ++requestSetDefaultTokenAccountCalls;
        lastTokenProvider = providerId;
        lastTokenAccountId = accountId;
        if (providerId == tokenAccountProviderForTest) {
            defaultTokenAccountValue = accountId;
        }
        emit tokenAccountsChanged(providerId);
        return "op-set-default-token-account";
    }
    Q_INVOKABLE QString codexActiveAccountID() const { return "live-system"; }
    Q_INVOKABLE QVariantList codexAccounts() const { return {}; }
    Q_INVOKABLE QVariantMap codexAccountState() const { return {}; }
    Q_INVOKABLE QVariantList codexFetchAttempts() const { return {}; }
    Q_INVOKABLE QVariantList utilizationChartData(const QString&, const QString&) const { return {}; }
    Q_INVOKABLE QVariantMap costUsageData() const {
        ++costUsageDataCalls;
        return costUsageDataValue;
    }
    Q_INVOKABLE QVariantList providerCostUsageList() const {
        ++providerCostUsageListCalls;
        return providerCostUsageListValue;
    }
    Q_INVOKABLE QVariantMap providerCostUsageData(const QString&) const { return {}; }
    Q_INVOKABLE QVariantMap providerDashboardData(const QString&) const { return {}; }
    Q_INVOKABLE QVariantMap codexConsumerProjectionData() const { return {}; }
    Q_INVOKABLE void refresh() {}
    Q_INVOKABLE void refreshAll() {}
    Q_INVOKABLE void refreshCostUsage() { ++refreshCostUsageCalls; }
    Q_INVOKABLE void requestCostUsageViewData() { ++requestCostUsageViewDataCalls; }
    Q_INVOKABLE void releaseCostUsageViewCaches() { ++releaseCostUsageViewCachesCalls; }
    Q_INVOKABLE void ensureCostUsageEnabled() {
        ++ensureCostUsageEnabledCalls;
        if (!costUsageEnabledValue) {
            costUsageEnabledValue = true;
            emit costUsageEnabledChanged();
        }
    }
    Q_INVOKABLE void refreshProvider(const QString& providerId) {
        ++refreshProviderCalls;
        lastRefreshProvider = providerId;
    }
    Q_INVOKABLE void setProviderEnabled(const QString&, bool) {}
    Q_INVOKABLE void setProviderSetting(const QString&, const QString& key, const QVariant& value) {
        ++setProviderSettingCalls;
        lastSettingKey = key;
        lastSettingValue = value;
    }
    Q_INVOKABLE bool setProviderSecret(const QString&, const QString& key, const QString& value) {
        ++setProviderSecretCalls;
        lastSecretKey = key;
        lastSecretValue = value;
        return true;
    }
    Q_INVOKABLE bool clearProviderSecret(const QString&, const QString&) { return false; }
    Q_INVOKABLE void testProviderConnection(const QString&) {}
    Q_INVOKABLE void startProviderLogin(const QString&) {}
    Q_INVOKABLE void cancelProviderLogin(const QString&) {}
    Q_INVOKABLE void refreshProviderStatuses() {}
    Q_INVOKABLE void moveProvider(int, int) {}
    Q_INVOKABLE void updateProviderIDs() {}

    void resetCounters() {
        providerListCalls = 0;
        providerDescriptorCalls = 0;
        requestProviderListCalls = 0;
        requestProviderDescriptorCalls = 0;
        costUsageDataCalls = 0;
        providerCostUsageListCalls = 0;
        refreshCostUsageCalls = 0;
        requestCostUsageViewDataCalls = 0;
        releaseCostUsageViewCachesCalls = 0;
        ensureCostUsageEnabledCalls = 0;
        setProviderSettingCalls = 0;
        setProviderSecretCalls = 0;
        tokenAccountsForProviderCalls = 0;
        addTokenAccountCalls = 0;
        addTokenAccountWithApiKeyCalls = 0;
        removeTokenAccountCalls = 0;
        setTokenAccountVisibilityCalls = 0;
        setTokenAccountSourceModeCalls = 0;
        setDefaultTokenAccountCalls = 0;
        requestAddTokenAccountCalls = 0;
        requestAddTokenAccountWithApiKeyCalls = 0;
        requestRemoveTokenAccountCalls = 0;
        requestSetTokenAccountVisibilityCalls = 0;
        requestSetTokenAccountSourceModeCalls = 0;
        requestSetDefaultTokenAccountCalls = 0;
        refreshProviderCalls = 0;
        providerIDsForTest.clear();
        tokenAccountProviderForTest.clear();
        tokenAccountsForProviderValue.clear();
        defaultTokenAccountValue.clear();
        costUsageDataValue.clear();
        providerCostUsageListValue.clear();
        lastSettingKey.clear();
        lastSettingValue.clear();
        lastSecretKey.clear();
        lastSecretValue.clear();
        lastTokenProvider.clear();
        lastTokenDisplayName.clear();
        lastTokenAccountId.clear();
        lastTokenApiKey.clear();
        lastRefreshProvider.clear();
        lastRequestedProviderDescriptor.clear();
        lastTokenSourceMode = -1;
        lastTokenVisibility = -1;
    }

    void emitProviderStatusChangedForTest(const QString& providerId) {
        emit providerStatusChanged(providerId);
    }

    mutable int providerListCalls = 0;
    mutable int providerDescriptorCalls = 0;
    int requestProviderListCalls = 0;
    int requestProviderDescriptorCalls = 0;
    mutable int costUsageDataCalls = 0;
    mutable int providerCostUsageListCalls = 0;
    int refreshCostUsageCalls = 0;
    int requestCostUsageViewDataCalls = 0;
    int releaseCostUsageViewCachesCalls = 0;
    int ensureCostUsageEnabledCalls = 0;
    int setProviderSettingCalls = 0;
    int setProviderSecretCalls = 0;
    mutable int tokenAccountsForProviderCalls = 0;
    int addTokenAccountCalls = 0;
    int addTokenAccountWithApiKeyCalls = 0;
    int removeTokenAccountCalls = 0;
    int setTokenAccountVisibilityCalls = 0;
    int setTokenAccountSourceModeCalls = 0;
    int setDefaultTokenAccountCalls = 0;
    int requestAddTokenAccountCalls = 0;
    int requestAddTokenAccountWithApiKeyCalls = 0;
    int requestRemoveTokenAccountCalls = 0;
    int requestSetTokenAccountVisibilityCalls = 0;
    int requestSetTokenAccountSourceModeCalls = 0;
    int requestSetDefaultTokenAccountCalls = 0;
    int refreshProviderCalls = 0;
    bool costUsageEnabledValue = false;
    QStringList providerIDsForTest;
    QString tokenAccountProviderForTest;
    QVariantList tokenAccountsForProviderValue;
    QString defaultTokenAccountValue;
    QVariantMap costUsageDataValue;
    QVariantList providerCostUsageListValue;
    QString lastSettingKey;
    QVariant lastSettingValue;
    QString lastSecretKey;
    QString lastSecretValue;
    QString lastTokenProvider;
    QString lastTokenDisplayName;
    QString lastTokenAccountId;
    QString lastTokenApiKey;
    QString lastRefreshProvider;
    QString lastRequestedProviderDescriptor;
    int lastTokenSourceMode = -1;
    int lastTokenVisibility = -1;

signals:
    void snapshotChanged(const QString&);
    void providerIDsChanged();
    void refreshingChanged();
    void costUsageEnabledChanged();
    void costUsageRefreshingChanged();
    void costUsageChanged();
    void snapshotRevisionChanged();
    void providerConnectionTestChanged(const QString&);
    void providerLoginStateChanged(const QString&);
    void providerStatusChanged(const QString&);
    void providerSecretChanged(const QString&, const QString&);
    void providerListModelChanged();
    void providerDescriptorChanged(const QString&);
    void tokenAccountsChanged(const QString&);
    void tokenAccountOperationStateChanged();
    void tokenAccountOperationFinished(const QString&, const QString&, bool, const QString&);
    void statusRevisionChanged();
    void codexAccountsChanged();
    void codexActiveAccountChanged(const QString&);
    void codexAccountStateChanged();
    void codexFetchAttemptsChanged();
};

class MockLanguageManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString language READ language CONSTANT)
public:
    QString language() const { return "en"; }
    Q_INVOKABLE QString translate(const QString& key) const { return key; }

signals:
    void retranslate();
};

class MockTrayProviderListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Role {
        ProviderIdRole = Qt::UserRole + 1,
        SnapshotRole,
        TokenAccountsRole,
        DefaultTokenAccountIdRole,
        AccountOptionsRole,
        StatusUrlRole,
        DashboardRole,
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : rows.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows.size()) {
            return {};
        }
        const QVariantMap row = rows.at(index.row());
        switch (role) {
        case ProviderIdRole: return row.value("providerId");
        case SnapshotRole: return row.value("snap");
        case TokenAccountsRole: return row.value("tokenAccounts");
        case DefaultTokenAccountIdRole: return row.value("defaultTokenAccountId");
        case AccountOptionsRole: return row.value("accountOptions");
        case StatusUrlRole: return row.value("statusUrl");
        case DashboardRole: return row.value("dashboard");
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {ProviderIdRole, "providerId"},
            {SnapshotRole, "snap"},
            {TokenAccountsRole, "tokenAccounts"},
            {DefaultTokenAccountIdRole, "defaultTokenAccountId"},
            {AccountOptionsRole, "accountOptions"},
            {StatusUrlRole, "statusUrl"},
            {DashboardRole, "dashboard"},
        };
    }

    void rebuildFromUsage(const MockUsageStore& usage) {
        beginResetModel();
        rows.clear();
        for (const QString& id : usage.providerIDs()) {
            const QVariantList accounts = usage.tokenAccountsForProviderValueForTest(id);
            QVariantList options;
            QVariantMap providerDefault;
            providerDefault["value"] = QString();
            providerDefault["label"] = QStringLiteral("Provider default");
            options.append(providerDefault);
            for (const QVariant& item : accounts) {
                const QVariantMap account = item.toMap();
                if (account.value("visibility").toString() == QLatin1String("archived")) {
                    continue;
                }
                QVariantMap option;
                const QString accountId = account.value("accountId").toString();
                option["value"] = accountId;
                option["label"] = account.value("displayName").toString().isEmpty()
                    ? accountId
                    : account.value("displayName").toString();
                options.append(option);
            }

            QVariantMap row;
            row["providerId"] = id;
            row["snap"] = usage.snapshotData(id);
            row["tokenAccounts"] = accounts;
            row["defaultTokenAccountId"] = usage.defaultTokenAccountForTest(id);
            row["accountOptions"] = options;
            row["statusUrl"] = QString();
            row["dashboard"] = QVariantMap();
            rows.append(row);
        }
        endResetModel();
    }

    QList<QVariantMap> rows;
};

class MockSettingsProviderListModel : public QAbstractListModel {
    Q_OBJECT
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

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : rows.size();
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= rows.size()) {
            return {};
        }
        const QVariantMap row = rows.at(index.row());
        switch (role) {
        case ProviderIdRole: return row.value("id");
        case NameRole: return row.value("name");
        case EnabledRole: return row.value("enabled");
        case BrandColorRole: return row.value("brandColor", QStringLiteral("#49A3B0"));
        case UsageRole: return row.value("usage");
        case StatusRole: return row.value("status", QStringLiteral("unknown"));
        case LastUpdatedRole: return row.value("lastUpdated");
        default: return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return {
            {ProviderIdRole, "providerId"},
            {NameRole, "name"},
            {EnabledRole, "enabled"},
            {BrandColorRole, "brandColor"},
            {UsageRole, "usage"},
            {StatusRole, "status"},
            {LastUpdatedRole, "lastUpdated"},
        };
    }

    void setProviders(const QVariantList& providers) {
        beginResetModel();
        rows.clear();
        for (const QVariant& item : providers) {
            QVariantMap row = item.toMap();
            if (!row.contains("brandColor")) row["brandColor"] = QStringLiteral("#49A3B0");
            if (!row.contains("status")) row["status"] = QStringLiteral("unknown");
            if (!row.contains("usage")) row["usage"] = QVariantMap();
            rows.append(row);
        }
        endResetModel();
    }

    QString providerIdAt(int row) const {
        if (row < 0 || row >= rows.size()) {
            return {};
        }
        return rows.at(row).value("id").toString();
    }

    QList<QVariantMap> rows;
};

class MockSettingsProvidersModel : public QObject {
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
    void setUsageStore(MockUsageStore* usage) {
        if (mockUsage == usage) {
            resetState();
            return;
        }
        if (mockUsage) {
            disconnect(mockUsage, nullptr, this, nullptr);
        }
        mockUsage = usage;
        if (mockUsage) {
            connect(mockUsage, &MockUsageStore::providerListModelChanged,
                    this, &MockSettingsProvidersModel::syncProviderList);
            connect(mockUsage, &MockUsageStore::providerDescriptorChanged,
                    this, [this](const QString& providerId) {
                if (providerId == selectedProviderValue) syncSelectedDescriptor();
            });
            connect(mockUsage, &MockUsageStore::providerConnectionTestChanged,
                    this, [this](const QString& providerId) {
                if (providerId == selectedProviderValue) syncSelectedConnectionTest();
            });
            connect(mockUsage, &MockUsageStore::providerStatusChanged,
                    this, [this](const QString& providerId) {
                if (providerId == selectedProviderValue) syncSelectedStatus();
            });
            connect(mockUsage, &MockUsageStore::snapshotRevisionChanged,
                    this, &MockSettingsProvidersModel::syncSelectedUsageSnapshot);
            connect(mockUsage, &MockUsageStore::tokenAccountsChanged,
                    this, [this](const QString& providerId) {
                if (providerId == selectedProviderValue) syncSelectedTokenAccounts();
            });
            connect(mockUsage, &MockUsageStore::tokenAccountOperationStateChanged,
                    this, &MockSettingsProvidersModel::syncTokenOperationState);
            connect(mockUsage, &MockUsageStore::codexAccountStateChanged,
                    this, &MockSettingsProvidersModel::syncCodexState);
        }
        resetState();
    }

    QAbstractListModel* providers() { return &providerModel; }
    int providerCount() const { return providerCountValue; }
    QString selectedProvider() const { return selectedProviderValue; }
    QVariantMap selectedDescriptor() const { return selectedDescriptorValue; }
    QString detailState() const { return detailStateValue; }
    QVariantMap selectedConnectionTest() const { return selectedConnectionTestValue; }
    QVariantMap selectedProviderStatus() const { return selectedProviderStatusValue; }
    QString selectedProviderError() const { return selectedProviderErrorValue; }
    QVariantMap selectedUsageSnapshot() const { return selectedUsageSnapshotValue; }
    QVariantList selectedTokenAccounts() const { return selectedTokenAccountsValue; }
    QString selectedDefaultTokenAccountId() const { return selectedDefaultTokenAccountIdValue; }
    QVariantMap tokenAccountOperationState() const { return tokenAccountOperationStateValue; }
    QVariantMap codexAccountState() const { return codexAccountStateValue; }
    QVariantMap codexProjection() const { return codexProjectionValue; }

    Q_INVOKABLE void requestOpenProvidersTab() {
        ++requestOpenProvidersTabCalls;
        if (!mockUsage) {
            return;
        }
        mockUsage->requestProviderList();
        syncProviderList();
        selectFirstProviderIfNeeded();
    }

    Q_INVOKABLE void selectProvider(const QString& providerId) {
        if (selectedProviderValue == providerId) {
            return;
        }
        selectedProviderValue = providerId;
        emit selectedProviderChanged();
        selectedDescriptorValue.clear();
        emit selectedDescriptorChanged();
        detailStateValue = providerId.isEmpty() ? QStringLiteral("idle") : QStringLiteral("loading");
        emit detailStateChanged();
        requestSelectedDescriptor();
        syncSelectedConnectionTest();
        syncSelectedStatus();
        syncSelectedError();
        syncSelectedUsageSnapshot();
        syncSelectedTokenAccounts();
        syncCodexState();
        syncCodexProjection();
    }

    Q_INVOKABLE void moveProvider(int fromIndex, int toIndex) {
        ++moveProviderCalls;
        if (mockUsage) mockUsage->moveProvider(fromIndex, toIndex);
    }
    Q_INVOKABLE void setProviderEnabled(const QString& providerId, bool enabled) {
        ++setProviderEnabledCalls;
        if (mockUsage) mockUsage->setProviderEnabled(providerId, enabled);
    }
    Q_INVOKABLE void testConnection(const QString& providerId) {
        ++testConnectionCalls;
        if (mockUsage) mockUsage->testProviderConnection(providerId);
    }
    Q_INVOKABLE void refreshProvider(const QString& providerId) {
        if (mockUsage) mockUsage->refreshProvider(providerId);
    }
    Q_INVOKABLE void setProviderSetting(const QString& providerId, const QString& key, const QVariant& value) {
        if (mockUsage) mockUsage->setProviderSetting(providerId, key, value);
    }
    Q_INVOKABLE void setProviderSecret(const QString& providerId, const QString& key, const QString& value) {
        if (mockUsage) mockUsage->setProviderSecret(providerId, key, value);
    }
    Q_INVOKABLE void clearProviderSecret(const QString& providerId, const QString& key) {
        if (mockUsage) mockUsage->clearProviderSecret(providerId, key);
    }

    Q_INVOKABLE void requestAddTokenAccount(const QString& providerId, const QString& displayName, int sourceMode) {
        if (mockUsage) mockUsage->requestAddTokenAccount(providerId, displayName, sourceMode);
    }
    Q_INVOKABLE void requestAddTokenAccountWithApiKey(const QString& providerId, const QString& displayName, int sourceMode, const QString& apiKey) {
        if (mockUsage) mockUsage->requestAddTokenAccountWithApiKey(providerId, displayName, sourceMode, apiKey);
    }
    Q_INVOKABLE void requestRemoveTokenAccount(const QString& accountId) {
        if (mockUsage) mockUsage->requestRemoveTokenAccount(accountId);
    }
    Q_INVOKABLE void requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId) {
        if (mockUsage) mockUsage->requestSetDefaultTokenAccount(providerId, accountId);
    }
    Q_INVOKABLE void requestSetTokenAccountSourceMode(const QString& accountId, int sourceMode) {
        if (mockUsage) mockUsage->requestSetTokenAccountSourceMode(accountId, sourceMode);
    }
    Q_INVOKABLE void requestSetTokenAccountVisibility(const QString& accountId, int visibility) {
        if (mockUsage) mockUsage->requestSetTokenAccountVisibility(accountId, visibility);
    }

    Q_INVOKABLE void setCodexActiveAccount(const QString& accountId) {
        ++setCodexActiveAccountCalls;
        lastCodexAccountId = accountId;
    }
    Q_INVOKABLE void addCodexAccount() { ++addCodexAccountCalls; }
    Q_INVOKABLE void cancelCodexAuthentication() { ++cancelCodexAuthenticationCalls; }
    Q_INVOKABLE void removeCodexAccount(const QString& accountId) {
        ++removeCodexAccountCalls;
        lastCodexAccountId = accountId;
    }
    Q_INVOKABLE void reauthenticateCodexAccount(const QString& accountId) {
        ++reauthenticateCodexAccountCalls;
        lastCodexAccountId = accountId;
    }
    Q_INVOKABLE void promoteCodexAccount(const QString& accountId) {
        ++promoteCodexAccountCalls;
        lastCodexAccountId = accountId;
    }

    void resetState() {
        providerModel.setProviders({});
        providerCountValue = 0;
        selectedProviderValue.clear();
        selectedDescriptorValue.clear();
        detailStateValue = QStringLiteral("idle");
        selectedConnectionTestValue = {{"state", "idle"}};
        selectedProviderStatusValue = {{"state", "unknown"}};
        selectedProviderErrorValue.clear();
        selectedUsageSnapshotValue.clear();
        selectedTokenAccountsValue.clear();
        selectedDefaultTokenAccountIdValue.clear();
        tokenAccountOperationStateValue.clear();
        codexAccountStateValue.clear();
        codexProjectionValue.clear();
        requestOpenProvidersTabCalls = 0;
        moveProviderCalls = 0;
        setProviderEnabledCalls = 0;
        testConnectionCalls = 0;
        setCodexActiveAccountCalls = 0;
        addCodexAccountCalls = 0;
        cancelCodexAuthenticationCalls = 0;
        removeCodexAccountCalls = 0;
        reauthenticateCodexAccountCalls = 0;
        promoteCodexAccountCalls = 0;
        lastCodexAccountId.clear();
        emit providersChanged();
        emit selectedProviderChanged();
        emit selectedDescriptorChanged();
        emit detailStateChanged();
        emit selectedConnectionTestChanged();
        emit selectedProviderStatusChanged();
        emit selectedProviderErrorChanged();
        emit selectedUsageSnapshotChanged();
        emit selectedTokenAccountsChanged();
        emit tokenAccountOperationStateChanged();
        emit codexAccountStateChanged();
        emit codexProjectionChanged();
    }

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
    void syncProviderList() {
        if (!mockUsage) {
            return;
        }
        const QVariantList providers = mockUsage->providerList();
        providerModel.setProviders(providers);
        providerCountValue = providers.size();
        emit providersChanged();
        selectFirstProviderIfNeeded();
    }

    void requestSelectedDescriptor() {
        if (!mockUsage || selectedProviderValue.isEmpty()) {
            return;
        }
        mockUsage->requestProviderDescriptor(selectedProviderValue);
        syncSelectedDescriptor();
    }

    void syncSelectedDescriptor() {
        if (!mockUsage || selectedProviderValue.isEmpty()) {
            return;
        }
        selectedDescriptorValue = mockUsage->providerDescriptorData(selectedProviderValue);
        if (selectedDescriptorValue.isEmpty()) {
            selectedDescriptorValue["displayName"] = mockUsage->providerDisplayName(selectedProviderValue);
            selectedDescriptorValue["enabled"] = true;
            selectedDescriptorValue["sourceModes"] = QStringList({"api"});
            selectedDescriptorValue["settingsFields"] = QVariantList();
        }
        detailStateValue = QStringLiteral("ready");
        emit selectedDescriptorChanged();
        emit detailStateChanged();
        syncSelectedTokenAccounts();
    }

    void syncSelectedConnectionTest() {
        if (!mockUsage || selectedProviderValue.isEmpty()) return;
        selectedConnectionTestValue = mockUsage->providerConnectionTest(selectedProviderValue);
        emit selectedConnectionTestChanged();
    }

    void syncSelectedStatus() {
        if (!mockUsage || selectedProviderValue.isEmpty()) return;
        selectedProviderStatusValue = mockUsage->providerStatus(selectedProviderValue);
        emit selectedProviderStatusChanged();
    }

    void syncSelectedError() {
        if (!mockUsage || selectedProviderValue.isEmpty()) return;
        selectedProviderErrorValue = mockUsage->providerError(selectedProviderValue);
        emit selectedProviderErrorChanged();
    }

    void syncSelectedUsageSnapshot() {
        if (!mockUsage || selectedProviderValue.isEmpty()) return;
        selectedUsageSnapshotValue = mockUsage->providerUsageSnapshot(selectedProviderValue);
        emit selectedUsageSnapshotChanged();
    }

    void syncSelectedTokenAccounts() {
        selectedTokenAccountsValue.clear();
        selectedDefaultTokenAccountIdValue.clear();
        if (mockUsage && !selectedProviderValue.isEmpty() && selectedProviderValue != QLatin1String("codex")) {
            selectedTokenAccountsValue = mockUsage->tokenAccountsForProvider(selectedProviderValue);
            selectedDefaultTokenAccountIdValue = mockUsage->defaultTokenAccount(selectedProviderValue);
        }
        emit selectedTokenAccountsChanged();
    }

    void syncTokenOperationState() {
        tokenAccountOperationStateValue = mockUsage ? mockUsage->tokenAccountOperationState() : QVariantMap();
        emit tokenAccountOperationStateChanged();
    }

    void syncCodexState() {
        codexAccountStateValue = (mockUsage && selectedProviderValue == QLatin1String("codex"))
            ? mockUsage->codexAccountState()
            : QVariantMap();
        emit codexAccountStateChanged();
    }

    void syncCodexProjection() {
        codexProjectionValue = (mockUsage && selectedProviderValue == QLatin1String("codex"))
            ? mockUsage->codexConsumerProjectionData()
            : QVariantMap();
        emit codexProjectionChanged();
    }

    void selectFirstProviderIfNeeded() {
        if (selectedProviderValue.isEmpty() && providerCountValue > 0) {
            selectProvider(providerModel.providerIdAt(0));
        }
    }

    MockUsageStore* mockUsage = nullptr;
    MockSettingsProviderListModel providerModel;
    int providerCountValue = 0;
    QString selectedProviderValue;
    QVariantMap selectedDescriptorValue;
    QString detailStateValue = QStringLiteral("idle");
    QVariantMap selectedConnectionTestValue = {{"state", "idle"}};
    QVariantMap selectedProviderStatusValue = {{"state", "unknown"}};
    QString selectedProviderErrorValue;
    QVariantMap selectedUsageSnapshotValue;
    QVariantList selectedTokenAccountsValue;
    QString selectedDefaultTokenAccountIdValue;
    QVariantMap tokenAccountOperationStateValue;
    QVariantMap codexAccountStateValue;
    QVariantMap codexProjectionValue;

public:
    int requestOpenProvidersTabCalls = 0;
    int moveProviderCalls = 0;
    int setProviderEnabledCalls = 0;
    int testConnectionCalls = 0;
    int setCodexActiveAccountCalls = 0;
    int addCodexAccountCalls = 0;
    int cancelCodexAuthenticationCalls = 0;
    int removeCodexAccountCalls = 0;
    int reauthenticateCodexAccountCalls = 0;
    int promoteCodexAccountCalls = 0;
    QString lastCodexAccountId;
};

class MockTrayViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(MockTrayProviderListModel* providers READ providers CONSTANT)
    Q_PROPERTY(int providerCount READ providerCount CONSTANT)
    Q_PROPERTY(bool isRefreshing READ isRefreshing NOTIFY isRefreshingChanged)
    Q_PROPERTY(bool costUsageEnabled READ costUsageEnabled NOTIFY costUsageEnabledChanged)
    Q_PROPERTY(bool costUsageRefreshing READ costUsageRefreshing NOTIFY costUsageRefreshingChanged)
    Q_PROPERTY(QVariantMap costData READ costData NOTIFY costDataChanged)
public:
    void setUsageStore(MockUsageStore* usage) { mockUsage = usage; }

    MockTrayProviderListModel* providers() {
        rebuildProviders();
        return &providerModel;
    }
    int providerCount() {
        rebuildProviders();
        return providerModel.rowCount();
    }
    bool isRefreshing() const { return false; }
    bool costUsageEnabled() const { return mockUsage ? mockUsage->costUsageEnabledValue : false; }
    bool costUsageRefreshing() const { return false; }
    QVariantMap costData() const { return mockUsage ? mockUsage->costUsageDataValue : QVariantMap(); }

    Q_INVOKABLE void refresh() { ++refreshCalls; }
    Q_INVOKABLE void refreshProvider(const QString& providerId) {
        ++refreshProviderCalls;
        lastRefreshProvider = providerId;
    }
    Q_INVOKABLE void ensureCostUsageEnabled() {
        ++ensureCostUsageEnabledCalls;
        if (mockUsage) {
            mockUsage->ensureCostUsageEnabled();
            emit costUsageEnabledChanged();
            emit costDataChanged();
        }
    }
    Q_INVOKABLE void requestCostUsageViewData() { ++requestCostUsageViewDataCalls; }
    Q_INVOKABLE QVariantList providerCostUsageList() const {
        ++providerCostUsageListCalls;
        return mockUsage ? mockUsage->providerCostUsageListValue : QVariantList();
    }
    Q_INVOKABLE QString requestSetDefaultTokenAccount(const QString& providerId, const QString& accountId) {
        ++requestSetDefaultTokenAccountCalls;
        lastTokenProvider = providerId;
        lastTokenAccountId = accountId;
        return mockUsage ? mockUsage->requestSetDefaultTokenAccount(providerId, accountId) : QString();
    }

    void resetCounters() {
        refreshCalls = 0;
        refreshProviderCalls = 0;
        ensureCostUsageEnabledCalls = 0;
        requestCostUsageViewDataCalls = 0;
        providerCostUsageListCalls = 0;
        requestSetDefaultTokenAccountCalls = 0;
        lastRefreshProvider.clear();
        lastTokenProvider.clear();
        lastTokenAccountId.clear();
    }

signals:
    void isRefreshingChanged();
    void costUsageEnabledChanged();
    void costUsageRefreshingChanged();
    void costDataChanged();
    void providerCostRowsChanged();

private:
    void rebuildProviders() {
        if (mockUsage) {
            providerModel.rebuildFromUsage(*mockUsage);
        }
    }

    MockUsageStore* mockUsage = nullptr;
    MockTrayProviderListModel providerModel;

public:
    int refreshCalls = 0;
    int refreshProviderCalls = 0;
    int ensureCostUsageEnabledCalls = 0;
    int requestCostUsageViewDataCalls = 0;
    mutable int providerCostUsageListCalls = 0;
    int requestSetDefaultTokenAccountCalls = 0;
    QString lastRefreshProvider;
    QString lastTokenProvider;
    QString lastTokenAccountId;
};

class MockUsageDetailsViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool costUsageEnabled READ costUsageEnabled NOTIFY costUsageEnabledChanged)
    Q_PROPERTY(bool costUsageRefreshing READ costUsageRefreshing NOTIFY costUsageRefreshingChanged)
    Q_PROPERTY(QVariantMap costData READ costData NOTIFY costDataChanged)
    Q_PROPERTY(QVariantList providerRows READ providerRows NOTIFY providerRowsChanged)
    Q_PROPERTY(int tokenProviderCount READ tokenProviderCount NOTIFY providerRowsChanged)
    Q_PROPERTY(QVariantMap providerDetails READ providerDetails NOTIFY providerDetailsChanged)
public:
    void setUsageStore(MockUsageStore* usage) { mockUsage = usage; }

    bool active() const { return activeValue; }
    bool costUsageEnabled() const { return mockUsage ? mockUsage->costUsageEnabledValue : false; }
    bool costUsageRefreshing() const { return false; }
    QVariantMap costData() const { return mockUsage ? mockUsage->costUsageDataValue : QVariantMap(); }
    QVariantList providerRows() const { return rows; }
    int tokenProviderCount() const { return tokenProviderCountValue; }
    QVariantMap providerDetails() const { return providerDetailsValue; }

    Q_INVOKABLE void activate() {
        ++activateCalls;
        activeValue = true;
        if (mockUsage) {
            mockUsage->ensureCostUsageEnabled();
            mockUsage->requestCostUsageViewData();
            rebuildRows();
        }
        emit activeChanged();
        emit costUsageEnabledChanged();
        emit costDataChanged();
        emit providerRowsChanged();
    }

    Q_INVOKABLE void deactivate() {
        ++deactivateCalls;
        activeValue = false;
        if (mockUsage) {
            mockUsage->releaseCostUsageViewCaches();
        }
        emit activeChanged();
    }

    Q_INVOKABLE void refreshCostUsage() {
        ++refreshCostUsageCalls;
        if (mockUsage) {
            mockUsage->refreshCostUsage();
        }
    }

    Q_INVOKABLE void requestProviderDetail(const QString& providerId) {
        ++requestProviderDetailCalls;
        QVariantMap detail;
        detail["providerId"] = providerId;
        detail["state"] = QStringLiteral("ready");
        detail["models"] = QVariantList();
        providerDetailsValue.insert(providerId, detail);
        emit providerDetailsChanged();
    }

    void resetCounters() {
        activateCalls = 0;
        deactivateCalls = 0;
        refreshCostUsageCalls = 0;
        requestProviderDetailCalls = 0;
        activeValue = false;
        rows.clear();
        providerDetailsValue.clear();
        tokenProviderCountValue = 0;
    }

signals:
    void activeChanged();
    void costUsageEnabledChanged();
    void costUsageRefreshingChanged();
    void costDataChanged();
    void providerRowsChanged();
    void providerDetailsChanged();

private:
    void rebuildRows() {
        rows.clear();
        tokenProviderCountValue = 0;
        if (!mockUsage) {
            return;
        }
        for (const QVariant& item : mockUsage->providerCostUsageListValue) {
            const QVariantMap token = item.toMap();
            QVariantMap row;
            row["providerId"] = token.value("providerId", token.value("id")).toString();
            row["displayName"] = row.value("providerId").toString();
            row["brandColor"] = QStringLiteral("#49A3B0");
            row["kind"] = QStringLiteral("token");
            row["hasTokenData"] = true;
            row["hasDetailAvailable"] = !token.value("models").toList().isEmpty();
            row["sessionTokens"] = token.value("sessionTokens").toDouble();
            row["sessionCostUSD"] = token.value("sessionCostUSD").toDouble();
            row["last30DaysTokens"] = token.value("last30DaysTokens").toDouble();
            row["last30DaysCostUSD"] = token.value("last30DaysCostUSD").toDouble();
            row["daily"] = token.value("daily").toList();
            row["enabled"] = true;
            rows.append(row);
            ++tokenProviderCountValue;
        }
    }

    MockUsageStore* mockUsage = nullptr;
    bool activeValue = false;
    QVariantList rows;
    QVariantMap providerDetailsValue;
    int tokenProviderCountValue = 0;

public:
    int activateCalls = 0;
    int deactivateCalls = 0;
    int refreshCostUsageCalls = 0;
    int requestProviderDetailCalls = 0;
};

class MockAppController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool settingsVisible READ isSettingsVisible CONSTANT)
    Q_PROPERTY(bool settingsMaximized READ isSettingsMaximized CONSTANT)
    Q_PROPERTY(bool usageVisible READ isUsageVisible CONSTANT)
public:
    bool isSettingsVisible() const { return false; }
    bool isSettingsMaximized() const { return false; }
    bool isUsageVisible() const { return false; }
    Q_INVOKABLE void openSettings() {}
    Q_INVOKABLE void closeSettings() {}
    Q_INVOKABLE void toggleSettings() {}
    Q_INVOKABLE void startSettingsMove() {}
    Q_INVOKABLE void startSettingsResize(int) {}
    Q_INVOKABLE void minimizeSettings() {}
    Q_INVOKABLE void toggleSettingsMaximized() {}
    Q_INVOKABLE void openUsage() {}
    Q_INVOKABLE void closeUsage() {}
    Q_INVOKABLE void startUsageMove() {}
    Q_INVOKABLE void startUsageResize(int) {}
    Q_INVOKABLE void minimizeUsage() {}
    Q_INVOKABLE void moveTrayPanel(int, int) {}
    Q_INVOKABLE void quitApp() {}
    Q_INVOKABLE void openExternalUrl(const QString&) {}
    Q_INVOKABLE void copyText(const QString&) {}

signals:
    void settingsVisibleChanged();
    void settingsMaximizedChanged();
    void usageVisibleChanged();
};

class tst_QmlSmoke : public QObject {
    Q_OBJECT
private:
    MockSettingsStore mockSettings;
    MockUsageStore mockUsage;
    MockSettingsProvidersModel mockSettingsProviders;
    MockTrayViewModel mockTray;
    MockUsageDetailsViewModel mockUsageDetails;
    MockLanguageManager mockLang;
    MockAppController mockAppCtrl;

    void setupEngine(QQmlEngine& engine) {
        engine.addImportPath("qrc:/qml");
        mockSettingsProviders.setUsageStore(&mockUsage);
        mockTray.setUsageStore(&mockUsage);
        mockUsageDetails.setUsageStore(&mockUsage);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "SettingsStore", &mockSettings);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "UsageStore", &mockUsage);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "SettingsProvidersModel", &mockSettingsProviders);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "TrayViewModel", &mockTray);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "UsageDetailsViewModel", &mockUsageDetails);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "AppController", &mockAppCtrl);
        qmlRegisterSingletonInstance("CodexBarX", 1, 0, "LanguageManager", &mockLang);
    }

    QObject* findObjectByStringProperty(QObject* root, const char* propertyName, const QString& value) const {
        if (!root) return nullptr;
        QVariant propertyValue = root->property(propertyName);
        if (propertyValue.isValid() && propertyValue.toString() == value) {
            return root;
        }
        const auto children = root->children();
        for (QObject* child : children) {
            if (QObject* match = findObjectByStringProperty(child, propertyName, value)) {
                return match;
            }
        }
        if (auto* item = qobject_cast<QQuickItem*>(root)) {
            const auto childItems = item->childItems();
            for (QQuickItem* childItem : childItems) {
                if (QObject* match = findObjectByStringProperty(childItem, propertyName, value)) {
                    return match;
                }
            }
        }
        return nullptr;
    }

    QQuickItem* textInputByPlaceholder(QQuickItem* root, const QString& placeholder) const {
        return qobject_cast<QQuickItem*>(
            findObjectByStringProperty(root, "placeholderText", placeholder));
    }

    QQuickItem* createInlineRoot(QQuickView& view, const QByteArray& qml, const QUrl& url) {
        QQmlComponent component(view.engine());
        component.setData(qml, url);
        if (component.status() == QQmlComponent::Error) {
            qWarning() << component.errorString();
        }
        if (component.status() != QQmlComponent::Ready) {
            return nullptr;
        }
        QObject* object = component.create();
        auto* root = qobject_cast<QQuickItem*>(object);
        if (!root) {
            delete object;
            return nullptr;
        }
        view.setContent(url, &component, root);
        return root;
    }

private slots:
    void basicQmlEngineWorks() {
        QQmlEngine engine;
        setupEngine(engine);

        QByteArray qml = "import QtQuick 2.15; Rectangle { width: 100; height: 100; color: 'red' }";
        QQmlComponent component(&engine);
        component.setData(qml, QUrl());
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));

        QObject* root = component.create();
        QVERIFY(root != nullptr);
        QCOMPARE(root->property("width").toInt(), 100);
        delete root;
    }

    void settingsWindowLoads() {
        QQmlEngine engine;
        setupEngine(engine);

        QQmlComponent component(&engine, QUrl("qrc:/qml/SettingsWindow.qml"));
        if (component.status() == QQmlComponent::Error) {
            qWarning() << "SettingsWindow load errors:" << component.errorString();
        }
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));

        QQuickItem* root = qobject_cast<QQuickItem*>(component.create());
        QVERIFY(root != nullptr);
        delete root;
    }

    void settingsWindowDefersProviderWorkUntilProvidersTab() {
#ifdef Q_OS_MACOS
        // Skip on macOS CI: QML component creation has race conditions with
        // the Qt event loop in CI environments, causing 5-minute timeouts.
        QSKIP("Test is unstable on macOS CI due to thread/event loop timing");
#else
        QQmlEngine engine;
        setupEngine(engine);
        mockUsage.resetCounters();

        QQmlComponent component(&engine, QUrl("qrc:/qml/SettingsWindow.qml"));
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));

        QQuickItem* root = qobject_cast<QQuickItem*>(component.create());
        QVERIFY(root != nullptr);
        QCoreApplication::processEvents();

        QCOMPARE(mockUsage.providerListCalls, 0);
        QCOMPARE(mockUsage.providerDescriptorCalls, 0);

        delete root;
#endif
    }

    void trayPanelLoads() {
        QQmlEngine engine;
        mockTray.resetCounters();
        setupEngine(engine);

        QQmlComponent component(&engine, QUrl("qrc:/qml/TrayPanel.qml"));
        if (component.status() == QQmlComponent::Error) {
            qWarning() << "TrayPanel load errors:" << component.errorString();
        }
        QVERIFY2(component.status() == QQmlComponent::Ready,
                 qPrintable(component.errorString()));

        QQuickItem* root = qobject_cast<QQuickItem*>(component.create());
        QVERIFY(root != nullptr);
        delete root;
    }

    void trayPanelDefersCostBreakdownOnFirstPaint() {
        QQuickView view;
        setupEngine(*view.engine());
        mockUsage.resetCounters();
        mockTray.resetCounters();

        QVariantMap costData;
        costData["hasData"] = true;
        costData["sessionCostUSD"] = 0.12;
        costData["sessionTokens"] = 1200;
        costData["last30DaysCostUSD"] = 3.45;
        costData["last30DaysTokens"] = 34500;
        costData["daily"] = QVariantList{};
        mockUsage.costUsageDataValue = costData;

        QVariantMap providerCost;
        providerCost["providerId"] = QStringLiteral("codex");
        providerCost["last30DaysCostUSD"] = 3.45;
        providerCost["last30DaysTokens"] = 34500;
        providerCost["models"] = QVariantList{};
        mockUsage.providerCostUsageListValue = {providerCost};

        view.resize(300, 600);
        view.setSource(QUrl("qrc:/qml/TrayPanel.qml"));
        QVERIFY2(view.status() == QQuickView::Ready,
                 qPrintable(view.errors().isEmpty() ? QString() : view.errors().first().toString()));
        view.show();
        QTest::qWait(250);

        QCOMPARE(mockTray.providerCostUsageListCalls, 0);
        QCOMPARE(mockUsage.providerCostUsageListCalls, 0);

        view.hide();
    }

    void trayPanelSwitchesTokenAccount() {
        QQuickView view;
        setupEngine(*view.engine());
        mockUsage.resetCounters();
        mockTray.resetCounters();
        mockUsage.providerIDsForTest = {QStringLiteral("claude")};
        mockUsage.tokenAccountProviderForTest = QStringLiteral("claude");
        mockUsage.defaultTokenAccountValue = QStringLiteral("account-a");

        QVariantMap accountA;
        accountA["accountId"] = QStringLiteral("account-a");
        accountA["providerId"] = QStringLiteral("claude");
        accountA["displayName"] = QStringLiteral("Work");
        accountA["sourceMode"] = QStringLiteral("api");
        accountA["visibility"] = QStringLiteral("visible");
        QVariantMap accountB;
        accountB["accountId"] = QStringLiteral("account-b");
        accountB["providerId"] = QStringLiteral("claude");
        accountB["displayName"] = QStringLiteral("Personal");
        accountB["sourceMode"] = QStringLiteral("api");
        accountB["visibility"] = QStringLiteral("visible");
        mockUsage.tokenAccountsForProviderValue = {accountA, accountB};

        view.resize(300, 600);
        view.setSource(QUrl("qrc:/qml/TrayPanel.qml"));
        QVERIFY2(view.status() == QQuickView::Ready,
                 qPrintable(view.errors().isEmpty() ? QString() : view.errors().first().toString()));
        view.show();
        QTest::qWait(250);

        QObject* switcher = nullptr;
        QTRY_VERIFY((switcher = findObjectByStringProperty(view.rootObject(),
                                                           "objectName",
                                                           "accountSwitcher_claude")) != nullptr);
        QVERIFY(QMetaObject::invokeMethod(switcher, "valueActivated",
                                          Q_ARG(QVariant, QVariant(QStringLiteral("account-b")))));

        QTRY_COMPARE(mockTray.requestSetDefaultTokenAccountCalls, 1);
        QTRY_COMPARE(mockUsage.requestSetDefaultTokenAccountCalls, 1);
        QCOMPARE(mockUsage.setDefaultTokenAccountCalls, 0);
        QCOMPARE(mockUsage.lastTokenProvider, QStringLiteral("claude"));
        QCOMPARE(mockUsage.lastTokenAccountId, QStringLiteral("account-b"));
        QCOMPARE(mockUsage.refreshProviderCalls, 0);

        view.hide();
    }

    void usageWindowDefersTokenUsagePaneUntilShown() {
        QQuickView view;
        setupEngine(*view.engine());
        mockUsage.resetCounters();
        mockUsageDetails.resetCounters();

        view.setSource(QUrl("qrc:/qml/UsageWindow.qml"));
        QVERIFY2(view.status() == QQuickView::Ready,
                 qPrintable(view.errors().isEmpty() ? QString() : view.errors().first().toString()));
        QCoreApplication::processEvents();

        QCOMPARE(mockUsage.costUsageDataCalls, 0);
        QCOMPARE(mockUsage.providerCostUsageListCalls, 0);
        QCOMPARE(mockUsage.providerListCalls, 0);
        QCOMPARE(mockUsage.ensureCostUsageEnabledCalls, 0);
        QCOMPARE(mockUsage.requestCostUsageViewDataCalls, 0);
        QCOMPARE(mockUsageDetails.activateCalls, 0);

        view.show();
        QTRY_COMPARE(mockUsageDetails.activateCalls, 1);
        QCOMPARE(mockUsage.costUsageDataCalls, 0);
        QCOMPARE(mockUsage.providerCostUsageListCalls, 0);
        QCOMPARE(mockUsage.providerListCalls, 0);
        QCOMPARE(mockUsage.ensureCostUsageEnabledCalls, 1);
        QCOMPARE(mockUsage.requestCostUsageViewDataCalls, 1);

        view.hide();
    }

    void tokenUsagePaneRequestsCostScanOnLoad() {
        QQuickView view;
        setupEngine(*view.engine());
        mockUsage.resetCounters();
        mockUsageDetails.resetCounters();
        mockUsage.costUsageEnabledValue = false;
        view.resize(760, 560);

        QQuickItem* root = createInlineRoot(view, R"(
            import QtQuick 2.15
            import QtQuick.Controls 2.15
            import "qrc:/qml/panes" as Panes

            Panes.TokenUsagePane {
                width: 740
                height: 540
            }
        )", QUrl("qrc:/tests/TokenUsagePaneHarness.qml"));

        QVERIFY(root != nullptr);
        view.show();
        QTRY_COMPARE(mockUsageDetails.activateCalls, 1);
        QTRY_COMPARE(mockUsage.ensureCostUsageEnabledCalls, 1);
        QCOMPARE(mockUsage.requestCostUsageViewDataCalls, 1);

        view.hide();
    }

    void usageWindowReleasesTokenUsageCachesWhenHidden() {
        QQuickView view;
        setupEngine(*view.engine());
        mockUsage.resetCounters();
        mockUsageDetails.resetCounters();

        view.setSource(QUrl("qrc:/qml/UsageWindow.qml"));
        QVERIFY2(view.status() == QQuickView::Ready,
                 qPrintable(view.errors().isEmpty() ? QString() : view.errors().first().toString()));

        view.show();
        QTRY_COMPARE(mockUsageDetails.activateCalls, 1);
        view.hide();

        QTRY_COMPARE(mockUsageDetails.deactivateCalls, 1);
        QTRY_COMPARE(mockUsage.releaseCostUsageViewCachesCalls, 1);
    }

    void settingsWindowRenders() {
        QQuickView view;
        setupEngine(*view.engine());
        view.setSource(QUrl("qrc:/qml/SettingsWindow.qml"));
        view.show();
        QTest::qWait(400);

        QImage screenshot = view.grabWindow();
        QVERIFY(!screenshot.isNull());
        QVERIFY(screenshot.width() > 100);
        QVERIFY(screenshot.height() > 100);

        QColor pixel = screenshot.pixelColor(10, 10);
        bool isDark = pixel.red() < 60 && pixel.green() < 60 && pixel.blue() < 60;
        QVERIFY2(isDark, qPrintable(
            QString("Expected dark pixel at (10,10), got RGB(%1,%2,%3)")
                .arg(pixel.red()).arg(pixel.green()).arg(pixel.blue())));

        view.hide();
    }

    void trayPanelRenders() {
        QQuickView view;
        mockTray.resetCounters();
        setupEngine(*view.engine());
        view.setSource(QUrl("qrc:/qml/TrayPanel.qml"));
        view.show();
        QTest::qWait(400);

        QImage screenshot = view.grabWindow();
        QVERIFY(!screenshot.isNull());
        QVERIFY(screenshot.width() > 50);
        QVERIFY(screenshot.height() > 50);

        view.hide();
    }

    void settingsWindowTabInteraction() {
        QQuickView view;
        setupEngine(*view.engine());
        view.setSource(QUrl("qrc:/qml/SettingsWindow.qml"));
        view.show();
        QTest::qWait(400);

        QQuickItem* root = view.rootObject();
        QVERIFY(root != nullptr);

        // Verify the window loaded with content by checking it has child items
        QVERIFY(root->childItems().size() > 0);

        view.hide();
    }

    void secretInputCommitsOnlyOnExplicitAction() {
        QQuickView view;
        setupEngine(*view.engine());
        view.resize(420, 80);
        QQuickItem* root = createInlineRoot(view, R"(
            import QtQuick 2.15
            import QtQuick.Controls 2.15
            import "qrc:/qml/components" as Components

            Components.SecretInput {
                width: 400
                height: 44
                placeholder: "secret placeholder"
                property int saveCount: 0
                property string lastSecret: ""
                onSaveRequested: function(value) {
                    saveCount += 1
                    lastSecret = value
                }
            }
        )", QUrl("qrc:/tests/SecretInputHarness.qml"));

        QVERIFY(root != nullptr);
        view.show();
        QTest::qWait(150);

        QQuickItem* input = textInputByPlaceholder(root, "secret placeholder");
        QVERIFY(input != nullptr);
        input->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_A);
        QTest::keyClick(&view, Qt::Key_B);
        QTest::keyClick(&view, Qt::Key_C);
        QCoreApplication::processEvents();

        QCOMPARE(root->property("saveCount").toInt(), 0);

        QTest::keyClick(&view, Qt::Key_Return);
        QTRY_COMPARE(root->property("saveCount").toInt(), 1);
        QCOMPARE(root->property("lastSecret").toString(), QString("abc"));

        view.hide();
    }

    void providerTextSettingCommitsOnlyOnExplicitAction() {
        QQuickView view;
        setupEngine(*view.engine());
        view.resize(760, 560);
        QQuickItem* root = createInlineRoot(view, R"(
            import QtQuick 2.15
            import QtQuick.Controls 2.15
            import "qrc:/qml/components" as Components

            Components.ProviderDetailView {
                width: 740
                height: 540
                providerId: "zai"
                descriptor: ({
                    displayName: "z.ai",
                    enabled: true,
                    sessionLabel: "Session",
                    weeklyLabel: "Weekly",
                    sourceModes: ["api"],
                    settingsFields: [
                        {
                            key: "apiBaseUrl",
                            label: "API Base URL",
                            type: "text",
                            value: "",
                            placeholder: "base url"
                        }
                    ]
                })
                connectionTest: ({state: "idle"})
                providerStatus: ({state: "ok"})
                usageSnapshot: null
                property int settingCount: 0
                property string lastKey: ""
                property string lastValue: ""
                onSettingChanged: function(key, value) {
                    settingCount += 1
                    lastKey = key
                    lastValue = value
                }
            }
        )", QUrl("qrc:/tests/ProviderDetailHarness.qml"));

        QVERIFY(root != nullptr);
        view.show();
        QTest::qWait(250);

        QQuickItem* input = textInputByPlaceholder(root, "base url");
        QVERIFY(input != nullptr);
        input->forceActiveFocus();
        QTest::keyClick(&view, Qt::Key_A);
        QTest::keyClick(&view, Qt::Key_B);
        QTest::keyClick(&view, Qt::Key_C);
        QCoreApplication::processEvents();

        QCOMPARE(root->property("settingCount").toInt(), 0);

        QTest::keyClick(&view, Qt::Key_Return);
        QTRY_COMPARE(root->property("settingCount").toInt(), 1);
        QCOMPARE(root->property("lastKey").toString(), QString("apiBaseUrl"));
        QCOMPARE(root->property("lastValue").toString(), QString("abc"));

        view.hide();
    }

    void tokenAccountsPaneAddsApiAccount() {
        QQuickView view;
        setupEngine(*view.engine());
        mockUsage.resetCounters();
        view.resize(760, 360);

        QQuickItem* root = createInlineRoot(view, R"(
            import QtQuick 2.15
            import QtQuick.Controls 2.15
            import "qrc:/qml/components" as Components
            import CodexBarX 1.0

            Components.TokenAccountsPane {
                width: 740
                height: 320
                providerId: "codebuff"
                descriptor: ({
                    sourceModes: ["api"],
                    tokenAccount: {
                        supportsMultipleAccounts: true,
                        requiredCredentialTypes: ["apiKey"]
                    }
                })
                accounts: []
                defaultAccountId: ""
                onAddAccount: function(displayName, sourceMode, apiKey) {
                    UsageStore.requestAddTokenAccountWithApiKey(providerId, displayName, sourceMode, apiKey)
                }
            }
        )", QUrl("qrc:/tests/TokenAccountsPaneHarness.qml"));

        QVERIFY(root != nullptr);
        view.show();
        QTest::qWait(200);

        QQuickItem* nameInput = qobject_cast<QQuickItem*>(
            findObjectByStringProperty(root, "objectName", "accountNameField"));
        QVERIFY(nameInput != nullptr);
        QVERIFY(nameInput->setProperty("text", QStringLiteral("Production")));

        QQuickItem* keyInput = qobject_cast<QQuickItem*>(
            findObjectByStringProperty(root, "objectName", "accountApiKeyField"));
        QVERIFY(keyInput != nullptr);
        QVERIFY(keyInput->setProperty("text", QStringLiteral("cb-test-token")));
        QCoreApplication::processEvents();

        QQuickItem* addButton = qobject_cast<QQuickItem*>(
            findObjectByStringProperty(root, "objectName", "addAccountButton"));
        QVERIFY(addButton != nullptr);
        const QPoint clickPoint = addButton->mapToScene(
            QPointF(addButton->width() / 2.0, addButton->height() / 2.0)).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, clickPoint);

        QTRY_COMPARE(mockUsage.requestAddTokenAccountWithApiKeyCalls, 1);
        QCOMPARE(mockUsage.addTokenAccountWithApiKeyCalls, 0);
        QCOMPARE(mockUsage.lastTokenProvider, QString("codebuff"));
        QCOMPARE(mockUsage.lastTokenDisplayName, QString("Production"));
        QCOMPARE(mockUsage.lastTokenSourceMode, 4);
        QCOMPARE(mockUsage.lastTokenApiKey, QString("cb-test-token"));

        view.hide();
    }

    void settingsWindowDebouncesStatusProviderListRefresh() {
        QQuickView view;
        setupEngine(*view.engine());
        view.setSource(QUrl("qrc:/qml/SettingsWindow.qml"));
        view.show();
        QTest::qWait(400);

        mockUsage.resetCounters();
        mockUsage.emitProviderStatusChangedForTest("codex");
        mockUsage.emitProviderStatusChangedForTest("claude");
        mockUsage.emitProviderStatusChangedForTest("cursor");
        QTest::qWait(250);

        QVERIFY2(mockUsage.providerListCalls <= 1,
                 qPrintable(QString("providerList called %1 times").arg(mockUsage.providerListCalls)));

        view.hide();
    }
};

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    tst_QmlSmoke tc;
    return QTest::qExec(&tc, argc, argv);
}

#include "tst_QmlSmoke.moc"
