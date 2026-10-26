
#pragma once

#include <QDialog>
#include "DiagnosticsEngine.h"

class QLabel;
class QTableWidget;
class QPushButton;

class DiagnosticsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DiagnosticsDialog(QWidget *parent = nullptr);

private slots:
    void runChecks();
    void exportReport();

private:
    QLabel *scoreLabel = nullptr;
    QLabel *summaryLabel = nullptr;
    QTableWidget *resultsTable = nullptr;
    QPushButton *runButton = nullptr;
    QPushButton *closeButton = nullptr;

    DiagnosticReport currentReport;

    void displayReport(const DiagnosticReport &report);
    QString statusText(CheckStatus status) const;
    QString statusColor(CheckStatus status) const;
};

#pragma once
