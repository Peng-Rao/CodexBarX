#include "ClaudeSourcePlanner.h"
#include <QDebug>
#include <QStringList>

QString ClaudeFetchPlanStep::dataSourceLabel() const
{
    switch (dataSource) {
    case ClaudeDataSource::OAuth: return QStringLiteral("oauth");
    case ClaudeDataSource::CLI: return QStringLiteral("cli");
    case ClaudeDataSource::Web: return QStringLiteral("web");
    case ClaudeDataSource::Auto: return QStringLiteral("auto");
    }
    return QStringLiteral("unknown");
}

QString ClaudeFetchPlanStep::reasonDescription() const
{
    switch (reason) {
    case ClaudeSourcePlanReason::ExplicitSourceSelection:
        return QStringLiteral("User explicitly selected this source");
    case ClaudeSourcePlanReason::AppAutoPreferredOAuth:
        return QStringLiteral("Auto mode: OAuth preferred (has credentials)");
    case ClaudeSourcePlanReason::AppAutoFallbackCLI:
        return QStringLiteral("Auto mode: Fallback to CLI");
    case ClaudeSourcePlanReason::AppAutoFallbackWeb:
        return QStringLiteral("Auto mode: Fallback to Web");
    case ClaudeSourcePlanReason::CLIAutoPreferredWeb:
        return QStringLiteral("CLI runtime: Web preferred");
    case ClaudeSourcePlanReason::CLIAutoFallbackCLI:
        return QStringLiteral("CLI runtime: Fallback to CLI");
    case ClaudeSourcePlanReason::NoSourceAvailable:
        return QStringLiteral("No data source available");
    }
    return QString();
}

QVector<ClaudeFetchPlanStep> ClaudeFetchPlan::availableSteps() const
{
    QVector<ClaudeFetchPlanStep> result;
    for (const auto& step : orderedSteps) {
        if (step.isPlausiblyAvailable) {
            result.append(step);
        }
    }
    return result;
}

ClaudeFetchPlanStep* ClaudeFetchPlan::preferredStep()
{
    for (auto& step : orderedSteps) {
        if (step.isPlausiblyAvailable) {
            return &step;
        }
    }
    return nullptr;
}

const ClaudeFetchPlanStep* ClaudeFetchPlan::preferredStep() const
{
    for (const auto& step : orderedSteps) {
        if (step.isPlausiblyAvailable) {
            return &step;
        }
    }
    return nullptr;
}

bool ClaudeFetchPlan::isNoSourceAvailable() const
{
    return availableSteps().isEmpty();
}

QString ClaudeFetchPlan::orderLabel() const
{
    QStringList labels;
    for (const auto& step : orderedSteps) {
        labels.append(step.dataSourceLabel() + (step.isPlausiblyAvailable ? "" : "?"));
    }
    return labels.join(QStringLiteral(" → "));
}

QString ClaudeFetchPlan::diagnosticDescription() const
{
    QStringList lines;
    lines << QStringLiteral("ClaudeFetchPlan:");
    lines << QStringLiteral("  Input:");
    lines << QStringLiteral("    selectedSource: %1").arg(ClaudeSourcePlanner::dataSourceToString(input.selectedSource));
    lines << QStringLiteral("    hasOAuthCredentials: %1").arg(input.hasOAuthCredentials);
    lines << QStringLiteral("    hasCLI: %2").arg(input.hasCLI);
    lines << QStringLiteral("    hasWebSession: %3").arg(input.hasWebSession);
    lines << QStringLiteral("    isCLIRuntime: %4").arg(input.isCLIRuntime);
    lines << QStringLiteral("    webExtrasEnabled: %5").arg(input.webExtrasEnabled);
    lines << QStringLiteral("  Steps:");
    for (const auto& step : orderedSteps) {
        lines << QStringLiteral("    - %1 (available: %2, reason: %3)")
                     .arg(step.dataSourceLabel())
                     .arg(step.isPlausiblyAvailable ? "yes" : "no")
                     .arg(step.reasonDescription());
    }
    return lines.join(QStringLiteral("\n"));
}

QString ClaudeSourcePlanner::dataSourceToString(ClaudeDataSource source)
{
    switch (source) {
    case ClaudeDataSource::Auto: return QStringLiteral("auto");
    case ClaudeDataSource::OAuth: return QStringLiteral("oauth");
    case ClaudeDataSource::CLI: return QStringLiteral("cli");
    case ClaudeDataSource::Web: return QStringLiteral("web");
    }
    return QStringLiteral("unknown");
}

ClaudeDataSource ClaudeSourcePlanner::dataSourceFromString(const QString& str)
{
    QString lower = str.toLower();
    if (lower == QStringLiteral("oauth")) return ClaudeDataSource::OAuth;
    if (lower == QStringLiteral("cli")) return ClaudeDataSource::CLI;
    if (lower == QStringLiteral("web")) return ClaudeDataSource::Web;
    return ClaudeDataSource::Auto;
}

ClaudeFetchPlan ClaudeSourcePlanner::resolve(const ClaudeSourcePlanningInput& input)
{
    ClaudeFetchPlan plan;
    plan.input = input;

    if (input.selectedSource == ClaudeDataSource::Auto) {
        plan.orderedSteps = buildAutoSteps(input);
    } else {
        plan.orderedSteps = buildExplicitSteps(input.selectedSource, input);
    }

    // qDebug().noquote() << plan.diagnosticDescription();
    return plan;
}

QVector<ClaudeFetchPlanStep> ClaudeSourcePlanner::buildAutoSteps(const ClaudeSourcePlanningInput& input)
{
    QVector<ClaudeFetchPlanStep> steps;

    if (input.isCLIRuntime) {
        // CLI runtime: Web → CLI
        steps.append({ClaudeDataSource::Web, ClaudeSourcePlanReason::CLIAutoPreferredWeb, input.hasWebSession});
        steps.append({ClaudeDataSource::CLI, ClaudeSourcePlanReason::CLIAutoFallbackCLI, input.hasCLI});
    } else {
        // App runtime: OAuth → CLI → Web
        steps.append({ClaudeDataSource::OAuth, ClaudeSourcePlanReason::AppAutoPreferredOAuth, input.hasOAuthCredentials});
        steps.append({ClaudeDataSource::CLI, ClaudeSourcePlanReason::AppAutoFallbackCLI, input.hasCLI});
        steps.append({ClaudeDataSource::Web, ClaudeSourcePlanReason::AppAutoFallbackWeb, input.hasWebSession});
    }

    return steps;
}

QVector<ClaudeFetchPlanStep> ClaudeSourcePlanner::buildExplicitSteps(ClaudeDataSource source,
                                                                       const ClaudeSourcePlanningInput& input)
{
    QVector<ClaudeFetchPlanStep> steps;

    bool available = false;
    switch (source) {
    case ClaudeDataSource::OAuth:
        available = input.hasOAuthCredentials;
        break;
    case ClaudeDataSource::CLI:
        available = input.hasCLI;
        break;
    case ClaudeDataSource::Web:
        available = input.hasWebSession;
        break;
    default:
        break;
    }

    steps.append({source, ClaudeSourcePlanReason::ExplicitSourceSelection, available});
    return steps;
}
