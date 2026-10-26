
#pragma once

#include <QList>

enum class CheckStatus
{
    Passed,
    Warning,
    Failed,
    NotChecked
};

struct DiagnosticResult
{
    QString name;
    CheckStatus status = CheckStatus::NotChecked;
    QString summary;
    QString recommendation;
};

struct DiagnosticReport
{
    QList<DiagnosticResult> checks;

    int healthScore = -1;
    int passedCount = 0;
    int warningCount = 0;
    int failedCount = 0;
    int notCheckedCount = 0;

    QString overallStatus;
};

class DiagnosticsEngine
{
public:
    DiagnosticReport runDiagnostics();

private:
    void checkGraphicsDevices(DiagnosticReport &report);
    void checkOpenGL(DiagnosticReport &report);
    void checkVulkan(DiagnosticReport &report);

    void addResult(
        DiagnosticReport &report,
        const QString &name,
        CheckStatus status,
        const QString &summary,
        const QString &recommendation = {}
    );

    void calculateHealthScore(DiagnosticReport &report);

    QString readFile(const QString &path);
    bool commandExists(const QString &command);
};

