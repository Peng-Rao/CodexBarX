#pragma once

#include "ManagedCodexAccount.h"
#include "CodexAccountReconciliation.h"
#include "CodexSystemAccountObserver.h"
#include "CodexOpenAIWorkspaceResolver.h"
#include "CodexOpenAIWorkspaceIdentityCache.h"
#include "CodexLoginRunner.h"
#include "../../models/CodexUsageResponse.h"

#include <QObject>
#include <QString>
#include <QVector>
#include <QHash>
#include <QFuture>
#include <optional>

#include "CodexActiveSource.h"

class UsageBackend;

struct CodexVisibleAccount {
    QString id;
    QString displayName;
    QString email;
    QString workspaceLabel;
    bool isLive;
    bool isActive;
    QString storedAccountID;
    bool canReauthenticate = true;   // false for live system accounts
    bool canRemove = true;           // false for live system accounts
    CodexActiveSource selectionSource = CodexActiveSource::ManagedAccount;
};

class ManagedCodexAccountService : public QObject {
    Q_OBJECT

public:
    explicit ManagedCodexAccountService(const QHash<QString, QString>& env, QObject* parent = nullptr);
    ~ManagedCodexAccountService() override;

    // Account operations
    QVector<CodexVisibleAccount> visibleAccounts() const;
    QString activeAccountID() const;
    QString liveAccountID() const;

    // Account management
    bool addAccount(const QString& email, const QString& homePath);
    bool authenticateNewAccount();
    bool promoteAccount(const QString& accountID);
    bool removeAccount(const QString& accountID);
    bool setActiveAccount(const QString& accountID);
    bool reauthenticateAccount(const QString& accountID);
    void cancelAuthentication();

    // State
    bool isAuthenticating() const;
    bool isRemoving() const;
    bool isLoadingSnapshot() const { return m_isLoadingSnapshot; }
    QString authenticatingAccountID() const;
    QString removingAccountID() const;
    bool hasUnreadableStore() const;
    QString authMessage() const;
    QString authError() const;
    QString authVerificationUri() const;
    QString authUserCode() const;
    QString activeManagedHomePath() const;

    // Reconciliation
    void refresh();
    void refreshAsync();
    CodexAccountReconciliationSnapshot currentSnapshot() const;
    void applySnapshot(const CodexAccountReconciliationSnapshot& snapshot);

    // Dependencies
    void setBackend(UsageBackend* backend);

signals:
    void accountsChanged();
    void activeAccountChanged(const QString& accountID);
    void authenticationStarted(const QString& accountID);
    void authenticationFinished(const QString& accountID, bool success);
    void authenticationStateChanged();
    void removalStarted(const QString& accountID);
    void removalFinished(const QString& accountID, bool success);
    void snapshotLoaded();

private slots:
    void onLoginFinished(const CodexLoginRunner::Result& result);

private:
    QHash<QString, QString> m_env;
    ManagedCodexAccountStore m_store;
    CodexSystemAccountObserver m_observer;
    CodexOpenAIWorkspaceIdentityCache m_workspaceCache;
    CodexAccountReconciliationSnapshot m_snapshot;
    QString m_activeAccountID;
    bool m_isAuthenticating;
    bool m_isRemoving;
    bool m_isLoadingSnapshot = false;
    int m_snapshotLoadGeneration = 0;
    QString m_authenticatingAccountID;
    QString m_removingAccountID;
    QString m_authMessage;
    QString m_authError;
    QString m_authVerificationUri;
    QString m_authUserCode;

    UsageBackend* m_backend = nullptr;
    CodexLoginRunner* m_loginRunner = nullptr;
    QString m_pendingHomePath;

    void updateVisibleAccounts();
    QString resolveDisplayName(const ManagedCodexAccount& account) const;
    QString resolveDisplayName(const ObservedSystemCodexAccount& account) const;
    QString resolveEmailFromCredentials(const CodexOAuthCredentials& credentials) const;
    void resetAuthenticationStatus();
    void finalizeLoginSuccess(const QString& homePath);
    void finalizeLoginFailure(const QString& homePath, const QString& message = QString());
};
