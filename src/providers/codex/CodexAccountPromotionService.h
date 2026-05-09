#pragma once

#include "CodexAccountPromotionContext.h"
#include "CodexDisplacedLivePreservation.h"

#include <QObject>
#include <QString>
#include <QHash>
#include <functional>

class UsageBackend;

enum class PromotionOutcome {
    Promoted,           // Successfully promoted
    ConvergedNoOp,      // Same identity, no operation needed
    Failed              // Failed
};

enum class DisplacedLiveDisposition {
    None,                                       // No system account displaced
    AlreadyManaged,                             // Already a managed account
    Imported,                                   // Imported as new managed account
    Rejected                                    // Rejected protection (unreadable/API-only)
};

struct CodexAccountPromotionResult {
    PromotionOutcome outcome = PromotionOutcome::Failed;
    QString targetManagedAccountId;
    DisplacedLiveDisposition displacedDisposition = DisplacedLiveDisposition::None;
    QString displacedManagedAccountId;          // If imported
    bool didMutateLiveAuth = false;
    QString errorMessage;
};

class CodexAccountPromotionService : public QObject {
    Q_OBJECT

public:
    explicit CodexAccountPromotionService(QObject* parent = nullptr);
    ~CodexAccountPromotionService() override;

    // Dependencies
    void setBackend(UsageBackend* backend);
    void setEnvironment(const QHash<QString, QString>& env);

    // Promotion operations
    void promoteAsync(const QString& accountId);

    // State
    bool isPromoting() const { return m_isPromoting; }
    QString promotingAccountId() const { return m_promotingAccountId; }

signals:
    void promotionStarted(const QString& accountId);
    void promotionFinished(const QString& accountId, const CodexAccountPromotionResult& result);

private:
    UsageBackend* m_backend = nullptr;
    QHash<QString, QString> m_env;
    bool m_isPromoting = false;
    QString m_promotingAccountId;

    CodexAccountPromotionResult executePromotion(const QString& accountId);
    bool atomicSwapAuthFiles(
        const QString& sourceHomePath,
        const QString& targetHomePath,
        QString& errorMessage
    );
};
