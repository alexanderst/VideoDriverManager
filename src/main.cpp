#include <QApplication>
#include <QCoreApplication>
#include <QMainWindow>
#include <QWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QStatusBar>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QProcess>
#include <QDateTime>
#include <QBrush>
#include <QColor>
#include <QAbstractItemView>
#include <QRegularExpression>

#include <algorithm>

#include "DiagnosticsDialog.h"

struct VideoDriver
{
    QString pciAddress;
    QString vendor;
    QString device;
    QString driver;
    QString version;
    QString status;
    QString description;
    QString driverPath;
};

class VideoDriverManager : public QMainWindow
{
    Q_OBJECT

public:
    explicit VideoDriverManager(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("Video Driver Manager");
        resize(1050, 620);
        setMinimumSize(800, 450);

        createInterface();
        refreshDrivers();
    }

private:
    QTableWidget *driverTable = nullptr;

    QLabel *deviceLabel = nullptr;
    QLabel *vendorLabel = nullptr;
    QLabel *driverLabel = nullptr;
    QLabel *versionLabel = nullptr;
    QLabel *statusLabel = nullptr;
    QLabel *pciLabel = nullptr;

    QPushButton *refreshButton = nullptr;

    QList<VideoDriver> drivers;

    void createInterface()
    {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);

        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        QLabel *title = new QLabel(
            "<h2>Graphics Hardware &amp; Drivers</h2>"
            "<p>Detected graphics devices and kernel driver status.</p>"
        );

        QPushButton *diagnosticsButton = new QPushButton("Run Diagnostics");

        mainLayout->addWidget(title);

        // Graphics device table
        driverTable = new QTableWidget(0, 5, this);

        driverTable->setHorizontalHeaderLabels({
            "Graphics Device",
            "Vendor",
            "Kernel Driver",
            "Version",
            "Status"
        });

        driverTable->setSelectionBehavior(
            QAbstractItemView::SelectRows
        );

        driverTable->setSelectionMode(
            QAbstractItemView::SingleSelection
        );

        driverTable->setEditTriggers(
            QAbstractItemView::NoEditTriggers
        );

        driverTable->setAlternatingRowColors(true);
        driverTable->setShowGrid(false);
        driverTable->verticalHeader()->setVisible(false);

        driverTable->horizontalHeader()->setSectionResizeMode(
            QHeaderView::Stretch
        );

        mainLayout->addWidget(driverTable, 3);

        // Driver details
        QGroupBox *detailsGroup = new QGroupBox("Driver Details");
        QFormLayout *detailsLayout = new QFormLayout(detailsGroup);

        deviceLabel = new QLabel("-");
        vendorLabel = new QLabel("-");
        driverLabel = new QLabel("-");
        versionLabel = new QLabel("-");
        statusLabel = new QLabel("-");
        pciLabel = new QLabel("-");

        detailsLayout->addRow("Device:", deviceLabel);
        detailsLayout->addRow("Vendor:", vendorLabel);
        detailsLayout->addRow("Kernel Module:", driverLabel);
        detailsLayout->addRow("Module Version:", versionLabel);
        detailsLayout->addRow("Driver Status:", statusLabel);
        detailsLayout->addRow("PCI Address:", pciLabel);

        mainLayout->addWidget(detailsGroup, 1);

        // Buttons
        QHBoxLayout *buttonLayout = new QHBoxLayout();

        buttonLayout->addStretch();
        buttonLayout->addWidget(diagnosticsButton);
        buttonLayout->addWidget(refreshButton);
        //buttonLayout->addWidget(closeButton);


        refreshButton = new QPushButton("Refresh");
        QPushButton *closeButton = new QPushButton("Close");

        refreshButton->setMinimumWidth(110);
        closeButton->setMinimumWidth(110);

        buttonLayout->addWidget(refreshButton);
        buttonLayout->addWidget(closeButton);

        mainLayout->addLayout(buttonLayout);

        statusBar()->showMessage("Ready");

        connect(
            refreshButton,
            &QPushButton::clicked,
            this,
            &VideoDriverManager::refreshDrivers
        );

        connect(
            closeButton,
            &QPushButton::clicked,
            this,
            &QWidget::close
        );

        connect(
            driverTable,
            &QTableWidget::itemSelectionChanged,
            this,
            &VideoDriverManager::showSelectedDriver
        );

        connect(
    diagnosticsButton,
    &QPushButton::clicked,
    this,
    [this]()
    {
        DiagnosticsDialog dialog(this);
        dialog.exec();
    }
);

    }

    // Read a sysfs text attribute.
    QString readSysfs(const QString &path)
    {
        QFile file(path);

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return {};

        QTextStream stream(&file);

        return stream.readLine().trimmed();
    }

    // Obtain the PCI vendor name.
    QString vendorName(const QString &vendorId)
    {
        const QString id = vendorId.toLower();

        if (id == "0x8086")
            return "Intel";

        if (id == "0x10de")
            return "NVIDIA";

        if (id == "0x1002" || id == "0x1022")
            return "AMD";

        if (id == "0x15ad")
            return "VMware";

        if (id == "0x1234")
            return "Virtual Device";

        return "Unknown (" + vendorId + ")";
    }

    // Read the version reported by the kernel module.
    QString moduleVersion(const QString &module)
    {
        if (module.isEmpty())
            return "N/A";

        QString path =
            "/sys/module/" + module + "/version";

        QString version = readSysfs(path);

        if (version.isEmpty())
            version = "Built-in / not reported";

        return version;
    }

    // Query the Linux kernel's list of loaded modules.
    bool isModuleLoaded(const QString &module)
    {
        if (module.isEmpty())
            return false;

        QFile file("/proc/modules");

        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return false;

        QTextStream stream(&file);

        while (!stream.atEnd())
        {
            QString line = stream.readLine();
            QString name = line.section(' ', 0, 0);

            if (name == module)
                return true;
        }

        return false;
    }

    // Determine whether a device is being used by the
    // active DRM graphics subsystem.
    bool isDrmDevice(const QString &pciAddress)
    {
        const QString path =
            "/sys/bus/pci/devices/" +
            pciAddress + "/drm";

        QDir dir(path);

        return dir.exists() && !dir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
        ).isEmpty();
    }

    // Scan PCI devices for display controllers.
    QList<VideoDriver> detectVideoDrivers()
    {
        QList<VideoDriver> result;

        QDir pciDir("/sys/bus/pci/devices");

        const QStringList devices = pciDir.entryList(
            QDir::Dirs | QDir::NoDotAndDotDot
        );

        for (const QString &pciAddress : devices)
        {
            const QString basePath =
                "/sys/bus/pci/devices/" + pciAddress;

            QString classCode =
                readSysfs(basePath + "/class");

            // PCI base class 0x03 is display controller.
            // This includes VGA, 3D, and other display devices.
            if (!classCode.startsWith("0x03"))
                continue;

            VideoDriver d;

            d.pciAddress = pciAddress;

            QString vendorId =
                readSysfs(basePath + "/vendor");

            QString deviceId =
                readSysfs(basePath + "/device");

            d.vendor = vendorName(vendorId);

            d.description =
                d.vendor + " Graphics Device (" +
                deviceId + ")";

            // Read the kernel driver symlink.
            QFileInfo driverInfo(
                basePath + "/driver"
            );

            if (driverInfo.isSymLink())
            {
                d.driver = driverInfo.symLinkTarget();

                // The target can be an absolute path.
                d.driver = QFileInfo(d.driver).fileName();

                if (d.driver.isEmpty())
                    d.driver = "Unknown";
            }
            else
            {
                d.driver = "None";
            }

            d.driverPath =
                "/sys/bus/pci/devices/" +
                pciAddress + "/driver";

            if (d.driver == "None")
            {
                d.version = "N/A";
                d.status = "No Driver";
            }
            else
            {
                d.version = moduleVersion(d.driver);

                const bool loaded =
                    isModuleLoaded(d.driver);

                const bool active =
                    isDrmDevice(d.pciAddress);

                if (active)
                {
                    d.status = "Active";
                }
                else if (loaded)
                {
                    d.status = "Loaded";
                }
                else
                {
                    // A bound driver may be built into
                    // the kernel and absent from /proc/modules.
                    d.status = "Bound / Unknown";
                }
            }

            result.append(d);
        }

        return result;
    }

    QColor statusColor(const QString &status)
    {
        if (status == "Active")
            return QColor("#16803C");

        if (status == "Loaded")
            return QColor("#2474C6");

        if (status == "No Driver")
            return QColor("#D97706");

        return QColor("#6B7280");
    }

private slots:

    void refreshDrivers()
    {
        refreshButton->setEnabled(false);

        statusBar()->showMessage(
            "Scanning graphics devices..."
        );

        driverTable->setRowCount(0);
        drivers.clear();

        drivers = detectVideoDrivers();

        for (int row = 0; row < drivers.size(); ++row)
        {
            const VideoDriver &d = drivers.at(row);

            driverTable->insertRow(row);

            QString deviceName = d.description;

            QTableWidgetItem *deviceItem =
                new QTableWidgetItem(deviceName);

            QTableWidgetItem *vendorItem =
                new QTableWidgetItem(d.vendor);

            QTableWidgetItem *driverItem =
                new QTableWidgetItem(d.driver);

            QTableWidgetItem *versionItem =
                new QTableWidgetItem(d.version);

            QTableWidgetItem *statusItem =
                new QTableWidgetItem(d.status);

            statusItem->setForeground(
                QBrush(statusColor(d.status))
            );

            statusItem->setFont(
                QFont("Sans Serif", 9, QFont::DemiBold)
            );

            deviceItem->setData(
                Qt::UserRole,
                d.pciAddress
            );

            driverTable->setItem(row, 0, deviceItem);
            driverTable->setItem(row, 1, vendorItem);
            driverTable->setItem(row, 2, driverItem);
            driverTable->setItem(row, 3, versionItem);
            driverTable->setItem(row, 4, statusItem);
        }

        refreshButton->setEnabled(true);

        if (drivers.isEmpty())
        {
            statusBar()->showMessage(
                "No PCI graphics devices detected."
            );

            clearDetails();
        }
        else
        {
            statusBar()->showMessage(
                QString("Found %1 graphics device(s). "
                        "Last scan: %2")
                    .arg(drivers.size())
                    .arg(QDateTime::currentDateTime()
                             .toString("HH:mm:ss"))
            );

            driverTable->selectRow(0);
            showSelectedDriver();
        }
    }

    void showSelectedDriver()
    {
        const int row = driverTable->currentRow();

        if (row < 0 || row >= drivers.size())
        {
            clearDetails();
            return;
        }

        const VideoDriver &d = drivers.at(row);

        deviceLabel->setText(d.description);
        vendorLabel->setText(d.vendor);
        driverLabel->setText(d.driver);
        versionLabel->setText(d.version);
        statusLabel->setText(d.status);
        pciLabel->setText(d.pciAddress);

        statusLabel->setStyleSheet(
            "font-weight: bold; color: " +
            statusColor(d.status).name() + ";"
        );
    }

    void clearDetails()
    {
        deviceLabel->setText("-");
        vendorLabel->setText("-");
        driverLabel->setText("-");
        versionLabel->setText("-");
        statusLabel->setText("-");
        pciLabel->setText("-");
        statusLabel->setStyleSheet("");
    }
};

namespace
{
QString diagnosticStatusText(CheckStatus status)
{
    switch (status)
    {
    case CheckStatus::Passed:    return "PASS";
    case CheckStatus::Warning:   return "WARNING";
    case CheckStatus::Failed:    return "FAIL";
    case CheckStatus::NotChecked: return "NOT CHECKED";
    }
    return "UNKNOWN";
}

QStringList wrapCell(const QString &text, int width)
{
    const QString value = text.isEmpty() ? QString("-") : text;
    QStringList lines;
    QString line;

    const QStringList words = value.split(QRegularExpression("\\s+"),
                                          Qt::SkipEmptyParts);
    for (const QString &word : words)
    {
        if (line.isEmpty())
        {
            line = word;
        }
        else if (line.size() + 1 + word.size() <= width)
        {
            line += " " + word;
        }
        else
        {
            lines << line;
            line = word;
        }

        // Break unusually long tokens so a single path/error string does
        // not make the table wider than the terminal-friendly column.
        while (line.size() > width)
        {
            lines << line.left(width);
            line = line.mid(width);
        }
    }

    if (!line.isEmpty())
        lines << line;
    if (lines.isEmpty())
        lines << "-";
    return lines;
}

int runBatchDiagnostics()
{
    DiagnosticsEngine engine;
    const DiagnosticReport report = engine.runDiagnostics();

    const QStringList headers = {
        "Diagnostic", "Status", "Result", "Recommendation"
    };
    const int widths[] = {24, 12, 42, 48};

    auto border = [&]() {
        QString line = "+";
        for (int width : widths)
            line += QString(width + 2, '-') + "+";
        return line;
    };

    auto printRow = [&](const QStringList &cells) {
        QList<QStringList> wrapped;
        int rowHeight = 1;
        for (int i = 0; i < cells.size(); ++i)
        {
            wrapped << QStringList{};
            wrapped[i] = wrapCell(cells[i], widths[i]);
            rowHeight = qMax(rowHeight, wrapped[i].size());
        }

        for (int row = 0; row < rowHeight; ++row)
        {
            QString line = "|";
            for (int col = 0; col < cells.size(); ++col)
            {
                const QString value = row < wrapped[col].size()
                    ? wrapped[col][row] : QString();
                line += " " + value.leftJustified(widths[col]) + " |";
            }
            QTextStream(stdout) << line << Qt::endl;
        }
    };

    QTextStream out(stdout);
    out << border() << Qt::endl;
    printRow(headers);
    out << border() << Qt::endl;

    for (const DiagnosticResult &result : report.checks)
    {
        printRow({
            result.name,
            diagnosticStatusText(result.status),
            result.summary,
            result.recommendation
        });
        out << border() << Qt::endl;
    }

    out << "Summary: "
        << report.passedCount << " passed, "
        << report.warningCount << " warnings, "
        << report.failedCount << " failed, "
        << report.notCheckedCount << " not checked."
        << Qt::endl;

    return 0;
}
}

int main(int argc, char *argv[])
{
    bool batchMode = false;
    for (int i = 1; i < argc; ++i)
    {
        if (QString::fromLocal8Bit(argv[i]) == "--batch")
        {
            batchMode = true;
        }
        else
        {
            QTextStream(stderr) << "Unknown argument: "
                                << QString::fromLocal8Bit(argv[i])
                                << Qt::endl
                                << "Usage: " << argv[0]
                                << " [--batch]" << Qt::endl;
            return 2;
        }
    }

    if (batchMode)
    {
        QCoreApplication app(argc, argv);
        QCoreApplication::setApplicationName("Video Driver Manager");
        QCoreApplication::setApplicationVersion("1.0.0");
        return runBatchDiagnostics();
    }

    QApplication app(argc, argv);
    QApplication::setApplicationName("Video Driver Manager");
    QApplication::setApplicationVersion("1.0.0");

    VideoDriverManager window;
    window.show();

    return app.exec();
}

#include "main.moc"

