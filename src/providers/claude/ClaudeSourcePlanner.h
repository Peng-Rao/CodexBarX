#pragma once

#include <QString>
#include <QVector>
#include <optional>

enum class ClaudeDataSource {
    Auto,
    OAuth,
    CLI,
    Web
};

enum class ClaudeSourcePlanReason {
    ExplicitSourceSelection,
    AppAutoPreferredOAuth,
    AppAutoFallbackCLI,
    AppAutoFallbackWeb,
    CLIAutoPreferredWeb,
    CLIAutoFallbackCLI,
    NoSourceAvailable
};

struct ClaudeFetchPlanStep {
    ClaudeDataSource dataSource;
    ClaudeSourcePlanReason reason;
    bool isPlausiblyAvailable;

    QString dataSourceLabel() const;
    QString reasonDescription() const;
};

struct ClaudeSourcePlanningInput {
    ClaudeDataSource selectedSource = ClaudeDataSource::Auto;
    bool hasOAuthCredentials = false;
    bool hasCLI = false;
    bool hasWebSession = false;
    bool isCLIRuntime = false;
    bool webExtrasEnabled = false;
};

struct ClaudeFetchPlan {
    ClaudeSourcePlanningInput input;
    QVector<ClaudeFetchPlanStep> orderedSteps;

    QVector<ClaudeFetchPlanStep> availableSteps() const;
    ClaudeFetchPlanStep* preferredStep();
    const ClaudeFetchPlanStep* preferredStep() const;
    bool isNoSourceAvailable() const;
    QString orderLabel() const;
    QString diagnosticDescription() const;
};

class ClaudeSourcePlanner {
public:
    static ClaudeFetchPlan resolve(const ClaudeSourcePlanningInput& input);

    static QString dataSourceToString(ClaudeDataSource source);
    static ClaudeDataSource dataSourceFromString(const QString& str);

private:
    static QVector<ClaudeFetchPlanStep> buildAutoSteps(const ClaudeSourcePlanningInput& input);
    static QVector<ClaudeFetchPlanStep> buildExplicitSteps(ClaudeDataSource source,
                                                            const ClaudeSourcePlanningInput& input);
};
