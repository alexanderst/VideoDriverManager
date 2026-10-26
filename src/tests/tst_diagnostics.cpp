#include <QtTest>
#include <QTableWidget>
#include <QPushButton>
#include <QProcess>
#include "../DiagnosticsEngine.h"
#include "../DiagnosticsDialog.h"

class DiagnosticsTests : public QObject
{
    Q_OBJECT
private slots:
    void engineProducesExpectedChecks();
    void engineSummaryCountsAreConsistent();
    void engineHealthScoreIsInRangeOrUnavailable();
    void dialogHasExpectedColumns();
    void batchModePrintsTable();
    void unknownArgumentIsRejected();
};

void DiagnosticsTests::engineProducesExpectedChecks()
{
    DiagnosticsEngine engine;
    const DiagnosticReport report = engine.runDiagnostics();
    QVERIFY2(!report.checks.isEmpty(), "Expected at least one diagnostic check");
    QStringList names;
    for (const DiagnosticResult &check : report.checks)
    {
        QVERIFY(!check.name.trimmed().isEmpty());
        QVERIFY(!check.summary.trimmed().isEmpty());
        QVERIFY(check.status == CheckStatus::Passed || check.status == CheckStatus::Warning ||
                check.status == CheckStatus::Failed || check.status == CheckStatus::NotChecked);
        names << check.name;
    }
    QVERIFY(names.contains("Graphics hardware"));
}

void DiagnosticsTests::engineSummaryCountsAreConsistent()
{
    DiagnosticsEngine engine;
    const DiagnosticReport report = engine.runDiagnostics();
    QCOMPARE(report.passedCount + report.warningCount + report.failedCount +
             report.notCheckedCount, report.checks.size());
    int passed = 0, warning = 0, failed = 0, notChecked = 0;
    for (const DiagnosticResult &check : report.checks)
    {
        switch (check.status) {
        case CheckStatus::Passed: ++passed; break;
        case CheckStatus::Warning: ++warning; break;
        case CheckStatus::Failed: ++failed; break;
        case CheckStatus::NotChecked: ++notChecked; break;
        }
    }
    QCOMPARE(report.passedCount, passed);
    QCOMPARE(report.warningCount, warning);
    QCOMPARE(report.failedCount, failed);
    QCOMPARE(report.notCheckedCount, notChecked);
}

void DiagnosticsTests::engineHealthScoreIsInRangeOrUnavailable()
{
    DiagnosticsEngine engine;
    const DiagnosticReport report = engine.runDiagnostics();
    QVERIFY(report.healthScore == -1 || (report.healthScore >= 0 && report.healthScore <= 100));
    if (report.healthScore == -1)
        QCOMPARE(report.overallStatus, QString("Insufficient information"));
    else
        QVERIFY(!report.overallStatus.trimmed().isEmpty());
}

void DiagnosticsTests::dialogHasExpectedColumns()
{
    DiagnosticsDialog dialog;
    const auto *table = dialog.findChild<QTableWidget *>();
    QVERIFY(table);
    QCOMPARE(table->columnCount(), 5);
    QCOMPARE(table->horizontalHeaderItem(0)->text(), QString("Diagnostic"));
    QCOMPARE(table->horizontalHeaderItem(1)->text(), QString("Status"));
    QCOMPARE(table->horizontalHeaderItem(2)->text(), QString("Result"));
    QCOMPARE(table->horizontalHeaderItem(3)->text(), QString("Recommendation"));
    QCOMPARE(table->horizontalHeaderItem(4)->text(), QString("Action"));
}

void DiagnosticsTests::batchModePrintsTable()
{
    QProcess process;
    process.start(QString::fromLocal8Bit(APP_EXECUTABLE), {"--batch"});
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(60000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    const QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    QVERIFY(output.contains("Diagnostic"));
    QVERIFY(output.contains("Status"));
    QVERIFY(output.contains("Result"));
    QVERIFY(output.contains("Recommendation"));
    QVERIFY(output.contains("Summary:"));
}

void DiagnosticsTests::unknownArgumentIsRejected()
{
    QProcess process;
    process.start(QString::fromLocal8Bit(APP_EXECUTABLE), {"--invalid-test-argument"});
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 2);
    QVERIFY(QString::fromLocal8Bit(process.readAllStandardError()).contains("Usage:"));
}

QTEST_MAIN(DiagnosticsTests)
#include "tst_diagnostics.moc"
