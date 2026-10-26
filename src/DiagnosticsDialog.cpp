
#include "DiagnosticsDialog.h"

#include <QFileDialog>
#include <QFile>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextStream>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTextDocument>
#include <QTextTable>
#include <QTextTableFormat>
#include <QAbstractItemView>
#include <QBrush>
#include <QProcess>

DiagnosticsDialog::DiagnosticsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Graphics Health Diagnostics");
    resize(950, 600);
    setMinimumSize(700, 450);

    auto *layout = new QVBoxLayout(this);

    auto *title = new QLabel(
        "<h2>Graphics Health Advisor</h2>"
        "Analyze graphics hardware, drivers, and graphics APIs."
    );

    layout->addWidget(title);

    auto *summaryGroup = new QGroupBox("Diagnostic Summary");
    auto *summaryLayout = new QVBoxLayout(summaryGroup);

    scoreLabel = new QLabel("Health score: Not evaluated");
    summaryLabel = new QLabel("Run diagnostics to inspect this system.");

    scoreLabel->setStyleSheet(
        "font-size: 20px; font-weight: bold;"
    );

    summaryLabel->setWordWrap(true);

    summaryLayout->addWidget(scoreLabel);
    summaryLayout->addWidget(summaryLabel);

    layout->addWidget(summaryGroup);

    resultsTable = new QTableWidget(0, 5, this);

    resultsTable->setHorizontalHeaderLabels({
        "Diagnostic",
        "Status",
        "Result",
        "Recommendation",
        "Action"
    });

    resultsTable->setEditTriggers(
        QAbstractItemView::NoEditTriggers
    );

    resultsTable->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );

    resultsTable->setSelectionMode(
        QAbstractItemView::SingleSelection
    );

    resultsTable->setAlternatingRowColors(true);
    resultsTable->verticalHeader()->setVisible(false);

    resultsTable->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents
    );

    resultsTable->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch
    );

    resultsTable->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch
    );
    resultsTable->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::ResizeToContents
    );

    resultsTable->setWordWrap(true);

    layout->addWidget(resultsTable, 1);

    auto *buttons = new QHBoxLayout();

    runButton = new QPushButton("Run Diagnostics");
    closeButton = new QPushButton("Close");
    auto *exportButton = new QPushButton("Export Report");
    closeButton = new QPushButton("Close");

    buttons->addWidget(runButton);
    buttons->addWidget(exportButton);
    buttons->addStretch();
    buttons->addWidget(closeButton);

    layout->addLayout(buttons);

    connect(
        runButton,
        &QPushButton::clicked,
        this,
        &DiagnosticsDialog::runChecks
    );

    connect(
        exportButton,
        &QPushButton::clicked,
        this,
        &DiagnosticsDialog::exportReport
    );

    connect(
        closeButton,
        &QPushButton::clicked,
        this,
        &QDialog::accept
    );
}

QString DiagnosticsDialog::statusText(CheckStatus status) const
{
    switch (status)
    {
    case CheckStatus::Passed:
        return "PASS";

    case CheckStatus::Warning:
        return "WARNING";

    case CheckStatus::Failed:
        return "FAILED";

    case CheckStatus::NotChecked:
        return "NOT CHECKED";
    }

    return "UNKNOWN";
}

QString DiagnosticsDialog::statusColor(CheckStatus status) const
{
    switch (status)
    {
    case CheckStatus::Passed:
        return "#16803C";

    case CheckStatus::Warning:
        return "#B7791F";

    case CheckStatus::Failed:
        return "#C53030";

    case CheckStatus::NotChecked:
        return "#6B7280";
    }

    return "#6B7280";
}

void DiagnosticsDialog::runChecks()
{
    runButton->setEnabled(false);
    runButton->setText("Checking...");

    scoreLabel->setText("Health score: Running diagnostics...");
    summaryLabel->setText(
        "Checking graphics hardware and graphics APIs."
    );

    resultsTable->setRowCount(0);

    // The engine runs synchronously in this prototype.
    DiagnosticsEngine engine;
    currentReport = engine.runDiagnostics();

    displayReport(currentReport);

    runButton->setEnabled(true);
    runButton->setText("Run Diagnostics");
}

void DiagnosticsDialog::displayReport(
    const DiagnosticReport &report)
{
    if (report.healthScore >= 0)
    {
        scoreLabel->setText(
            QString("Diagnostic score: %1 / 100")
                .arg(report.healthScore)
        );
    }
    else
    {
        scoreLabel->setText(
            "Diagnostic score: Not available"
        );
    }

    summaryLabel->setText(
        QString("%1. Passed: %2 | Warnings: %3 | "
                "Failed: %4 | Not checked: %5")
            .arg(report.overallStatus)
            .arg(report.passedCount)
            .arg(report.warningCount)
            .arg(report.failedCount)
            .arg(report.notCheckedCount)
    );

    resultsTable->setRowCount(report.checks.size());

    for (int row = 0; row < report.checks.size(); ++row)
    {
        const DiagnosticResult &result = report.checks.at(row);

        auto *nameItem = new QTableWidgetItem(result.name);
        auto *statusItem =
            new QTableWidgetItem(statusText(result.status));
        auto *summaryItem =
            new QTableWidgetItem(result.summary);
        auto *recommendationItem =
            new QTableWidgetItem(
                result.recommendation.isEmpty()
                    ? "No action suggested."
                    : result.recommendation
            );

        statusItem->setForeground(
            QBrush(QColor(statusColor(result.status)))
        );

        statusItem->setFont(
            QFont("Sans Serif", 9, QFont::DemiBold)
        );

        resultsTable->setItem(row, 0, nameItem);
        resultsTable->setItem(row, 1, statusItem);
        resultsTable->setItem(row, 2, summaryItem);
        resultsTable->setItem(row, 3, recommendationItem);
    }

    resultsTable->resizeRowsToContents();
}

void DiagnosticsDialog::exportReport()
{
    if (currentReport.checks.isEmpty())
    {
        QMessageBox::information(
            this,
            "No Report",
            "Run diagnostics before exporting a report."
        );

        return;
    }

    const QString filename = QFileDialog::getSaveFileName(
        this,
        "Export Diagnostic Report",
        "graphics-diagnostic-report.html",
        "HTML Report (*.html)"
    );

    if (filename.isEmpty())
        return;

    QFile file(filename);

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::critical(
            this,
            "Export Failed",
            "Unable to write the report file."
        );

        return;
    }

    QTextStream out(&file);

    out << "<!DOCTYPE html><html><head>"
           "<meta charset=\"utf-8\">"
           "<title>Graphics Diagnostic Report</title>"
           "<style>"
           "body{font-family:sans-serif;margin:32px;color:#222}"
           "table{border-collapse:collapse;width:100%}"
           "th,td{border:1px solid #ddd;padding:10px;text-align:left}"
           "th{background:#f0f0f0}"
           "</style></head><body>";

    out << "<h1>Graphics Diagnostic Report</h1>";

    out << "<p>Generated: "
        << QDateTime::currentDateTime().toString(
               "yyyy-MM-dd HH:mm:ss"
           ).toHtmlEscaped()
        << "</p>";

    out << "<h2>Summary</h2><p>"
        << currentReport.overallStatus.toHtmlEscaped()
        << "</p>";

    if (currentReport.healthScore >= 0)
    {
        out << "<p>Diagnostic score: "
            << currentReport.healthScore
            << " / 100</p>";
    }
    else
    {
        out << "<p>Diagnostic score: Not available</p>";
    }

    out << "<table><thead><tr>"
           "<th>Diagnostic</th>"
           "<th>Status</th>"
           "<th>Result</th>"
           "<th>Recommendation</th>"
           "</tr></thead><tbody>";

    for (const DiagnosticResult &result : currentReport.checks)
    {
        out << "<tr><td>"
            << result.name.toHtmlEscaped()
            << "</td><td>"
            << statusText(result.status).toHtmlEscaped()
            << "</td><td>"
            << result.summary.toHtmlEscaped()
            << "</td><td>"
            << result.recommendation.toHtmlEscaped()
            << "</td></tr>";
    }

    out << "</tbody></table></body></html>";

    file.close();

    QMessageBox::information(
        this,
        "Report Exported",
        "The diagnostic report was saved successfully."
    );
}

