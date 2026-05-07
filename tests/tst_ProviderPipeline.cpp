#include <QtTest/QtTest>
#include <QThread>
#include <QSignalSpy>
#include <QElapsedTimer>

#include "../src/providers/IProvider.h"
#include "../src/providers/ProviderPipeline.h"

#include <utility>

class FakeStrategy : public IFetchStrategy {
public:
    static int destroyedCount;
    static int totalCreated;

    FakeStrategy(QString id, bool available, bool success, bool fallback,
                 int kind = ProviderFetchKind::APIToken, QString errorMsg = "")
        : m_id(std::move(id))
        , m_available(available)
        , m_success(success)
        , m_fallback(fallback)
        , m_kind(kind)
        , m_errorMsg(std::move(errorMsg))
    {
        ++totalCreated;
    }

    ~FakeStrategy() override {
        ++destroyedCount;
    }

    QString id() const override { return m_id; }
    int kind() const override { return m_kind; }
    bool isAvailable(const ProviderFetchContext&) const override { return m_available; }

    ProviderFetchResult fetchSync(const ProviderFetchContext&) override {
        ++calls;
        ProviderFetchResult result;
        result.strategyID = m_id;
        result.strategyKind = kind();
        result.sourceLabel = m_id;
        result.success = m_success;
        if (!m_success) {
            result.errorMessage = m_errorMsg.isEmpty() ? m_id + " failed" : m_errorMsg;
        }
        return result;
    }

    bool shouldFallback(const ProviderFetchResult&, const ProviderFetchContext&) const override {
        return m_fallback;
    }

    int calls = 0;

private:
    QString m_id;
    bool m_available;
    bool m_success;
    bool m_fallback;
    int m_kind;
    QString m_errorMsg;
};

int FakeStrategy::destroyedCount = 0;
int FakeStrategy::totalCreated = 0;

class SourceProvider : public IProvider {
public:
    QString id() const override { return "source"; }
    QString displayName() const override { return "Source"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("oauth", true, true, false, ProviderFetchKind::OAuth),
            new FakeStrategy("web", true, true, false, ProviderFetchKind::Web),
            new FakeStrategy("webdashboard", true, true, false, ProviderFetchKind::WebDashboard),
            new FakeStrategy("cli", true, true, false, ProviderFetchKind::CLI),
            new FakeStrategy("api", true, true, false, ProviderFetchKind::APIToken)
        };
    }
};

class FallbackProvider : public IProvider {
public:
    QString id() const override { return "fallback"; }
    QString displayName() const override { return "Fallback"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("first", true, false, true),
            new FakeStrategy("second", true, false, true),
            new FakeStrategy("third", true, true, false)
        };
    }
};

class NoFallbackProvider : public IProvider {
public:
    QString id() const override { return "nofallback"; }
    QString displayName() const override { return "NoFallback"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("first", true, false, false),
            new FakeStrategy("second", true, true, false)
        };
    }
};

class AllUnavailableProvider : public IProvider {
public:
    QString id() const override { return "allunavailable"; }
    QString displayName() const override { return "AllUnavailable"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("unavail1", false, true, false),
            new FakeStrategy("unavail2", false, true, false),
            new FakeStrategy("unavail3", false, true, false)
        };
    }
};

class AllFailProvider : public IProvider {
public:
    QString id() const override { return "allfail"; }
    QString displayName() const override { return "AllFail"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("fail1", true, false, true),
            new FakeStrategy("fail2", true, false, true),
            new FakeStrategy("fail3", true, false, true)
        };
    }
};

class EmptyErrorProvider : public IProvider {
public:
    QString id() const override { return "emptyerror"; }
    QString displayName() const override { return "EmptyError"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("emptyerror", true, false, true, ProviderFetchKind::APIToken, "")
        };
    }
};

class tst_ProviderPipeline : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        FakeStrategy::destroyedCount = 0;
        FakeStrategy::totalCreated = 0;
    }

    void cleanupTestCase() {
    }

    void init() {
        FakeStrategy::destroyedCount = 0;
        FakeStrategy::totalCreated = 0;
    }

    void skipsUnavailableAndReturnsSuccess() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy unavailable("unavailable", false, true, false);
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &unavailable, &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("success"));
        QCOMPARE(unavailable.calls, 0);
        QCOMPARE(success.calls, 1);
    }

    void fallsBackAfterFailure() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy first("first", true, false, true);
        FakeStrategy second("second", true, true, false);
        QVector<IFetchStrategy*> strategies = { &first, &second };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("second"));
        QCOMPARE(first.calls, 1);
        QCOMPARE(second.calls, 1);
    }

    void stopsWhenFallbackIsDisabled() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy first("first", true, false, false);
        FakeStrategy second("second", true, true, false);
        QVector<IFetchStrategy*> strategies = { &first, &second };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QString("first failed"));
        QCOMPARE(first.calls, 1);
        QCOMPARE(second.calls, 0);
    }

    void filtersStrategiesBySourceMode() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Web;
        SourceProvider provider;

        QVector<IFetchStrategy*> strategies = pipeline.resolveStrategies(&provider, ctx);

        QCOMPARE(strategies.size(), 2);
        QStringList ids;
        for (auto* s : strategies) ids << s->id();
        QVERIFY(ids.contains("web"));
        QVERIFY(ids.contains("webdashboard"));
        qDeleteAll(strategies);
    }

    void executeProviderOwnsAndDestroysResolvedStrategies() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::CLI;
        FallbackProvider provider;
        FakeStrategy::destroyedCount = 0;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("third"));
        QCOMPARE(FakeStrategy::destroyedCount, 3);
    }

    void handlesNullProvider() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;

        QVector<IFetchStrategy*> strategies = pipeline.resolveStrategies(nullptr, ctx);

        QVERIFY(strategies.isEmpty());
    }

    void handlesEmptyStrategyList() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;

        QVector<IFetchStrategy*> strategies;
        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(!result.success);
    }

    void handlesNullStrategyInList() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { nullptr, &success, nullptr };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("success"));
    }

    void handlesAllStrategiesUnavailable() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Auto;
        AllUnavailableProvider provider;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(!result.success);
        QCOMPARE(result.attempts.size(), 3);
        for (const auto& attempt : result.attempts) {
            QVERIFY(!attempt.available);
        }
    }

    void handlesAllStrategiesFailing() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Auto;
        AllFailProvider provider;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(!result.success);
        QCOMPARE(result.attempts.size(), 3);
        for (const auto& attempt : result.attempts) {
            QVERIFY(attempt.available);
            QVERIFY(!attempt.success);
        }
    }

    void handlesEmptyErrorMessage() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        EmptyErrorProvider provider;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(!result.success);
        QVERIFY(result.errorMessage.isEmpty());
    }

    void returnsFirstSuccessfulStrategy() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy first("first", true, true, false);
        FakeStrategy second("second", true, true, false);
        QVector<IFetchStrategy*> strategies = { &first, &second };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("first"));
        QCOMPARE(first.calls, 1);
        QCOMPARE(second.calls, 0);
    }

    void executesStrategiesInOrder() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Auto;
        FallbackProvider provider;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QCOMPARE(result.attempts.size(), 3);
        QCOMPARE(result.attempts[0].strategyID, QString("first"));
        QCOMPARE(result.attempts[1].strategyID, QString("second"));
        QCOMPARE(result.attempts[2].strategyID, QString("third"));
    }

    void recordsAllAttempts() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy first("first", true, false, true);
        FakeStrategy second("second", true, true, false);
        QVector<IFetchStrategy*> strategies = { &first, &second };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.attempts.size(), 2);
        QCOMPARE(result.attempts[0].strategyID, QString("first"));
        QVERIFY(!result.attempts[0].success);
        QCOMPARE(result.attempts[1].strategyID, QString("second"));
        QVERIFY(result.attempts[1].success);
    }

    void recordsUnavailableAttempts() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy unavailable("unavailable", false, true, false);
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &unavailable, &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.attempts.size(), 2);
        QVERIFY(!result.attempts[0].available);
        QVERIFY(result.attempts[1].available);
    }

    void emitsStrategyStartedSignal() {
        ProviderPipeline pipeline;
        QSignalSpy spy(&pipeline, &ProviderPipeline::strategyStarted);
        ProviderFetchContext ctx;
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &success };

        pipeline.execute(strategies, ctx);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QString("success"));
    }

    void emitsPipelineCompleteSignal() {
        ProviderPipeline pipeline;
        QSignalSpy spy(&pipeline, &ProviderPipeline::pipelineComplete);
        ProviderFetchContext ctx;
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(spy.count(), 1);
        ProviderFetchResult emittedResult = spy.first().first().value<ProviderFetchResult>();
        QCOMPARE(emittedResult.success, result.success);
        QCOMPARE(emittedResult.strategyID, result.strategyID);
    }

    void filtersStrategiesByCLIMode() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::CLI;
        SourceProvider provider;

        QVector<IFetchStrategy*> strategies = pipeline.resolveStrategies(&provider, ctx);

        QCOMPARE(strategies.size(), 1);
        QCOMPARE(strategies.first()->id(), QString("cli"));
        qDeleteAll(strategies);
    }

    void filtersStrategiesByOAuthMode() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::OAuth;
        SourceProvider provider;

        QVector<IFetchStrategy*> strategies = pipeline.resolveStrategies(&provider, ctx);

        QCOMPARE(strategies.size(), 1);
        QCOMPARE(strategies.first()->id(), QString("oauth"));
        qDeleteAll(strategies);
    }

    void filtersStrategiesByAPIMode() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::API;
        SourceProvider provider;

        QVector<IFetchStrategy*> strategies = pipeline.resolveStrategies(&provider, ctx);

        QCOMPARE(strategies.size(), 1);
        QCOMPARE(strategies.first()->id(), QString("api"));
        qDeleteAll(strategies);
    }

    void autoModeReturnsAllStrategies() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Auto;
        SourceProvider provider;

        QVector<IFetchStrategy*> strategies = pipeline.resolveStrategies(&provider, ctx);

        QCOMPARE(strategies.size(), 5);
        qDeleteAll(strategies);
    }

    void singleStrategySuccess() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy single("single", true, true, false);
        QVector<IFetchStrategy*> strategies = { &single };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("single"));
        QCOMPARE(single.calls, 1);
    }

    void singleStrategyFailure() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy single("single", true, false, false);
        QVector<IFetchStrategy*> strategies = { &single };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QString("single failed"));
    }

    void mixedAvailabilityStrategies() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy unavail1("unavail1", false, true, false);
        FakeStrategy unavail2("unavail2", false, true, false);
        FakeStrategy fail("fail", true, false, true);
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &unavail1, &unavail2, &fail, &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("success"));
        QCOMPARE(unavail1.calls, 0);
        QCOMPARE(unavail2.calls, 0);
        QCOMPARE(fail.calls, 1);
        QCOMPARE(success.calls, 1);
    }

    void strategyKindPreserved() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy oauth("oauth", true, true, false, ProviderFetchKind::OAuth);
        QVector<IFetchStrategy*> strategies = { &oauth };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.strategyKind, ProviderFetchKind::OAuth);
    }

    void multiplePipelinesIndependent() {
        ProviderPipeline pipeline1;
        ProviderPipeline pipeline2;
        ProviderFetchContext ctx;
        
        FakeStrategy success1("success1", true, true, false);
        FakeStrategy success2("success2", true, true, false);
        
        QVector<IFetchStrategy*> strategies1 = { &success1 };
        QVector<IFetchStrategy*> strategies2 = { &success2 };

        ProviderFetchResult result1 = pipeline1.execute(strategies1, ctx);
        ProviderFetchResult result2 = pipeline2.execute(strategies2, ctx);

        QCOMPARE(result1.strategyID, QString("success1"));
        QCOMPARE(result2.strategyID, QString("success2"));
    }

    void webDashboardKindIncludedInWebMode() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Web;
        
        FakeStrategy web("web", true, true, false, ProviderFetchKind::Web);
        FakeStrategy dashboard("dashboard", true, true, false, ProviderFetchKind::WebDashboard);
        FakeStrategy cli("cli", true, true, false, ProviderFetchKind::CLI);
        
        QVector<IFetchStrategy*> strategies = { &web, &dashboard, &cli };
        QVector<IFetchStrategy*> filtered;
        
        for (auto* s : strategies) {
            if (s->kind() == ProviderFetchKind::Web || s->kind() == ProviderFetchKind::WebDashboard) {
                filtered.append(s);
            }
        }

        QCOMPARE(filtered.size(), 2);
    }

    void attemptRecordsTimestamp() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &success };

        QDateTime before = QDateTime::currentDateTime();
        ProviderFetchResult result = pipeline.execute(strategies, ctx);
        QDateTime after = QDateTime::currentDateTime();

        QVERIFY(!result.attempts.isEmpty());
        QVERIFY(result.attempts.first().attemptedAt >= before);
        QVERIFY(result.attempts.first().attemptedAt <= after);
    }

    void fallbackChainExecutesAllUntilSuccess() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy fail1("fail1", true, false, true);
        FakeStrategy fail2("fail2", true, false, true);
        FakeStrategy fail3("fail3", true, false, true);
        FakeStrategy success("success", true, true, false);
        QVector<IFetchStrategy*> strategies = { &fail1, &fail2, &fail3, &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(result.success);
        QCOMPARE(fail1.calls, 1);
        QCOMPARE(fail2.calls, 1);
        QCOMPARE(fail3.calls, 1);
        QCOMPARE(success.calls, 1);
        QCOMPARE(result.attempts.size(), 4);
    }

    void stopsAtFirstNonFallbackFailure() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy fail1("fail1", true, false, true);
        FakeStrategy fail2("fail2", true, false, false);
        FakeStrategy shouldNotRun("shouldNotRun", true, true, false);
        QVector<IFetchStrategy*> strategies = { &fail1, &fail2, &shouldNotRun };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(!result.success);
        QCOMPARE(fail1.calls, 1);
        QCOMPARE(fail2.calls, 1);
        QCOMPARE(shouldNotRun.calls, 0);
    }

    void sourceLabelPreserved() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy success("id123", true, true, false);
        QVector<IFetchStrategy*> strategies = { &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.sourceLabel, QString("id123"));
    }

    void noAvailableStrategyErrorMessage() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Auto;
        AllUnavailableProvider provider;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(!result.success);
        QVERIFY(!result.errorMessage.isEmpty());
    }
};

QTEST_MAIN(tst_ProviderPipeline)
#include "tst_ProviderPipeline.moc"
