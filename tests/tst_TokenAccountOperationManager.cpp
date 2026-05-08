#include "account/TokenAccountOperationManager.h"
#include "account/TokenAccountStore.h"
#include "providers/shared/ProviderCredentialStore.h"

#include <QtTest/QtTest>

class tst_TokenAccountOperationManager : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanupTestCase();
    void synchronousAddAccountCreatesDefaultMetadata();
    void synchronousAddAccountWithApiKeyStoresCredentials();
    void synchronousMutationsUpdateStore();
};

void tst_TokenAccountOperationManager::init()
{
    ProviderCredentialStore::setBackendForTesting(std::make_shared<InMemoryCredentialBackend>());
    TokenAccountStore* store = TokenAccountStore::instance();
    for (const auto& account : store->allAccounts()) {
        store->removeAccount(account.accountId);
    }
}

void tst_TokenAccountOperationManager::cleanupTestCase()
{
    ProviderCredentialStore::resetBackendForTesting();
}

void tst_TokenAccountOperationManager::synchronousAddAccountCreatesDefaultMetadata()
{
    TokenAccountOperationManager manager;

    const QString accountId = manager.addAccount(QStringLiteral("openrouter"),
                                                 QStringLiteral("Personal"),
                                                 static_cast<int>(ProviderSourceMode::API));

    QVERIFY(!accountId.isEmpty());
    auto account = TokenAccountStore::instance()->accountMetadata(accountId);
    QVERIFY(account.has_value());
    QCOMPARE(account->providerId, QStringLiteral("openrouter"));
    QCOMPARE(account->displayName, QStringLiteral("Personal"));
    QCOMPARE(account->sourceMode, ProviderSourceMode::API);
    QCOMPARE(TokenAccountStore::instance()->defaultAccountId(QStringLiteral("openrouter")), accountId);
}

void tst_TokenAccountOperationManager::synchronousAddAccountWithApiKeyStoresCredentials()
{
    TokenAccountOperationManager manager;

    const QString accountId = manager.addAccountWithApiKey(QStringLiteral("openrouter"),
                                                           QString(),
                                                           static_cast<int>(ProviderSourceMode::API),
                                                           QStringLiteral("sk-test"));

    QVERIFY(!accountId.isEmpty());
    auto account = TokenAccountStore::instance()->accountWithCredentials(accountId);
    QVERIFY(account.has_value());
    QCOMPARE(account->displayName, QStringLiteral("openrouter"));
    QVERIFY(account->credentials.api.has_value());
    QCOMPARE(account->credentials.api->apiKey.data(), QByteArray("sk-test"));
}

void tst_TokenAccountOperationManager::synchronousMutationsUpdateStore()
{
    TokenAccountOperationManager manager;
    const QString accountId = manager.addAccount(QStringLiteral("openrouter"),
                                                 QStringLiteral("Work"),
                                                 static_cast<int>(ProviderSourceMode::API));

    QVERIFY(manager.setVisibility(accountId, static_cast<int>(AccountVisibility::Hidden)));
    QVERIFY(manager.setSourceMode(accountId, static_cast<int>(ProviderSourceMode::Web)));
    QVERIFY(manager.setDefault(QStringLiteral("openrouter"), QString()));

    auto account = TokenAccountStore::instance()->accountMetadata(accountId);
    QVERIFY(account.has_value());
    QCOMPARE(account->visibility, AccountVisibility::Hidden);
    QCOMPARE(account->sourceMode, ProviderSourceMode::Web);
    QVERIFY(TokenAccountStore::instance()->defaultAccountId(QStringLiteral("openrouter")).isEmpty());

    QVERIFY(manager.removeAccount(accountId));
    QVERIFY(!TokenAccountStore::instance()->accountMetadata(accountId).has_value());
}

QTEST_MAIN(tst_TokenAccountOperationManager)

#include "tst_TokenAccountOperationManager.moc"
