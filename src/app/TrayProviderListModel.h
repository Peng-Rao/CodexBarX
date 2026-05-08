#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

class UsageStore;

class TrayProviderListModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

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

    explicit TrayProviderListModel(QObject* parent = nullptr);

    void setUsageStore(UsageStore* store);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_rows.size(); }

public slots:
    void scheduleFullRebuild();
    void refreshProvider(const QString& providerId);

signals:
    void countChanged();

private:
    struct Row {
        QString providerId;
        QVariantMap snap;
        QVariantList tokenAccounts;
        QString defaultTokenAccountId;
        QVariantList accountOptions;
        QString statusUrl;
        QVariantMap dashboard;
    };

    Row buildRow(const QString& providerId) const;
    QVariantList buildAccountOptions(const QVariantList& tokenAccounts) const;
    void rebuildNow();
    void emitCountIfChanged(int previousCount);

    QPointer<UsageStore> m_store;
    QList<Row> m_rows;
    QHash<QString, int> m_indexByProvider;
    QTimer m_rebuildTimer;
};
