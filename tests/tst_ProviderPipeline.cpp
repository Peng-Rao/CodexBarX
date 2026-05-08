#include <QtTest/QtTest>
#include <QThread>

#include "../src/providers/IProvider.h"
#include "../src/providers/ProviderPipeline.h"

#include <utility>

class FakeStrategy : public IFetchStrategy {
public:
    static int destroyedCount;

    FakeStrategy(QString id, bool available, bool success, bool fallback,
                 int kind = ProviderFetchKind::APIToken)
        : m_id(std::move(id))
        , m_available(available)
        , m_success(success)
        , m_fallback(fallback)
        , m_kind(kind)
    {}

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
        if (!m_success) result.errorMessage = m_id + " failed";
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
};

int FakeStrategy::destroyedCount = 0;

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

class CleanupProvider : public IProvider {
public:
    QString id() const override { return "cleanup"; }
    QString displayName() const override { return "Cleanup"; }
    QString sessionLabel() const override { return "Session"; }
    QString weeklyLabel() const override { return "Weekly"; }
    bool defaultEnabled() const override { return false; }

    QVector<IFetchStrategy*> createStrategies(const ProviderFetchContext&) override {
        return {
            new FakeStrategy("first", true, false, true, ProviderFetchKind::CLI),
            new FakeStrategy("second", true, true, false, ProviderFetchKind::CLI),
            new FakeStrategy("web-filtered", true, true, false, ProviderFetchKind::Web)
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

class tst_ProviderPipeline : public QObject {
    Q_OBJECT
private slots:
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

    void executeProviderOwnsAndDestroysResolvedStrategies() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::CLI;
        CleanupProvider provider;
        FakeStrategy::destroyedCount = 0;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(result.success);
        QCOMPARE(result.strategyID, QString("second"));
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
    }

    void handlesAllStrategiesFailing() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        ctx.sourceMode = ProviderSourceMode::Auto;
        AllFailProvider provider;

        ProviderFetchResult result = pipeline.executeProvider(&provider, ctx);

        QVERIFY(!result.success);
        QCOMPARE(result.attempts.size(), 3);
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

    void recordsAllAttempts() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy first("first", true, false, true);
        FakeStrategy second("second", true, true, false);
        QVector<IFetchStrategy*> strategies = { &first, &second };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.attempts.size(), 2);
        QCOMPARE(result.attempts[0].strategyID, QString("first"));
        QCOMPARE(result.attempts[1].strategyID, QString("second"));
    }

    void strategyKindPreserved() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy oauth("oauth", true, true, false, ProviderFetchKind::OAuth);
        QVector<IFetchStrategy*> strategies = { &oauth };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.strategyKind, ProviderFetchKind::OAuth);
    }

    void sourceLabelPreserved() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;
        FakeStrategy success("id123", true, true, false);
        QVector<IFetchStrategy*> strategies = { &success };

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QCOMPARE(result.sourceLabel, QString("id123"));
    }

    void pipelineTimeoutEnforced() {
        ProviderPipeline pipeline;
        ProviderFetchContext ctx;

        class SlowStrategy : public IFetchStrategy {
        public:
            QString id() const override { return "slow"; }
            int kind() const override { return ProviderFetchKind::APIToken; }
            bool isAvailable(const ProviderFetchContext&) const override { return true; }
            ProviderFetchResult fetchSync(const ProviderFetchContext&) override {
                QThread::sleep(1);
                return {};
            }
            bool shouldFallback(const ProviderFetchResult&, const ProviderFetchContext&) const override {
                return true;
            }
        };

        int timeoutSec = ProviderPipeline::PIPELINE_TIMEOUT_MS / 1000;
        int strategyCount = timeoutSec + 5;
        QVector<IFetchStrategy*> strategies;
        for (int i = 0; i < strategyCount; ++i) {
            strategies.append(new SlowStrategy());
        }

        ProviderFetchResult result = pipeline.execute(strategies, ctx);

        QVERIFY(!result.success);
        QVERIFY(result.errorMessage.contains("timeout"));

        qDeleteAll(strategies);
    }
};

QTEST_MAIN(tst_ProviderPipeline)
#include "tst_ProviderPipeline.moc"
