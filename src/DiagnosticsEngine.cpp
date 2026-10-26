
#include "DiagnosticsEngine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

QString DiagnosticsEngine::readFile(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream stream(&file);
    return stream.readLine().trimmed();
}

bool DiagnosticsEngine::commandExists(const QString &command)
{
    const QStringList paths =
        qEnvironmentVariable("PATH").split(':');

    for (const QString &path : paths)
    {
        if (path.isEmpty())
            continue;

        QFileInfo info(path + "/" + command);

        if (info.isFile() && info.isExecutable())
            return true;
    }

    return false;
}

void DiagnosticsEngine::addResult(
    DiagnosticReport &report,
    const QString &name,
    CheckStatus status,
    const QString &summary,
    const QString &recommendation)
{
    report.checks.append({
        name,
        status,
        summary,
        recommendation
    });
}

void DiagnosticsEngine::checkGraphicsDevices(
    DiagnosticReport &report)
{
    QDir pciDir("/sys/bus/pci/devices");

    if (!pciDir.exists())
    {
        addResult(
            report,
            "Graphics hardware",
            CheckStatus::NotChecked,
            "PCI device information is unavailable.",
            "Check whether the Linux sysfs filesystem is mounted."
        );
        return;
    }

    const QStringList devices = pciDir.entryList(
        QDir::Dirs | QDir::NoDotAndDotDot
    );

    int gpuCount = 0;
    int boundCount = 0;
    int drmCount = 0;

    for (const QString &pci : devices)
    {
        const QString base =
            "/sys/bus/pci/devices/" + pci;

        const QString classCode =
            readFile(base + "/class");

        // PCI display controller class.
        if (!classCode.startsWith("0x03"))
            continue;

        ++gpuCount;

        const QFileInfo driverInfo(base + "/driver");

        if (driverInfo.isSymLink())
            ++boundCount;

        QDir drmDir(base + "/drm");

        if (drmDir.exists() &&
            !drmDir.entryList(
                QDir::Dirs | QDir::NoDotAndDotDot
            ).isEmpty())
        {
            ++drmCount;
        }
    }

    if (gpuCount == 0)
    {
        addResult(
            report,
            "Graphics hardware",
            CheckStatus::Failed,
            "No PCI display controllers were detected.",
            "If this is a virtual machine, check its graphics configuration. "
            "Otherwise, inspect the system's PCI devices and kernel logs."
        );
        return;
    }

    addResult(
        report,
        "Graphics hardware",
        CheckStatus::Passed,
        QString("Detected %1 PCI graphics device(s).")
            .arg(gpuCount)
    );

    if (boundCount == gpuCount)
    {
        addResult(
            report,
            "Kernel driver binding",
            CheckStatus::Passed,
            QString("A kernel driver is bound to all %1 device(s).")
                .arg(gpuCount)
        );
    }
    else
    {
        addResult(
            report,
            "Kernel driver binding",
            CheckStatus::Warning,
            QString("%1 of %2 graphics device(s) have a bound driver.")
                .arg(boundCount)
                .arg(gpuCount),
            "Inspect the affected device's driver binding and kernel logs. "
            "Some devices may intentionally be unused."
        );
    }

    if (drmCount > 0)
    {
        addResult(
            report,
            "DRM graphics subsystem",
            CheckStatus::Passed,
            QString("%1 device(s) expose a DRM graphics interface.")
                .arg(drmCount)
        );
    }
    else
    {
        addResult(
            report,
            "DRM graphics subsystem",
            CheckStatus::Warning,
            "No DRM device was found for the detected PCI graphics devices.",
            "Check kernel driver initialization and the system's graphics "
            "configuration. This check does not cover every possible setup."
        );
    }
}

void DiagnosticsEngine::checkOpenGL(DiagnosticReport &report)
{
    if (!qEnvironmentVariableIsSet("DISPLAY") &&
        !qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
    {
        addResult(
            report,
            "OpenGL",
            CheckStatus::NotChecked,
            "No graphical display session was detected.",
            "Run the OpenGL test inside a graphical desktop session."
        );
        return;
    }

    if (!commandExists("glxinfo"))
    {
        addResult(
            report,
            "OpenGL",
            CheckStatus::NotChecked,
            "The glxinfo utility is not installed.",
            "Install it with: sudo apt install mesa-utils"
        );
        return;
    }

    QProcess process;

    process.setProgram("glxinfo");
    process.setArguments({"-B"});
    process.start();

    if (!process.waitForStarted(1500))
    {
        addResult(
            report,
            "OpenGL",
            CheckStatus::NotChecked,
            "Could not start glxinfo.",
            "Check the mesa-utils installation."
        );
        return;
    }

    if (!process.waitForFinished(4000))
    {
        process.kill();
        process.waitForFinished(500);

        addResult(
            report,
            "OpenGL",
            CheckStatus::NotChecked,
            "The OpenGL query timed out.",
            "Check the graphical session and graphics driver configuration."
        );
        return;
    }

    const QString output =
        QString::fromLocal8Bit(process.readAllStandardOutput()) +
        QString::fromLocal8Bit(process.readAllStandardError());

    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0)
    {
        addResult(
            report,
            "OpenGL",
            CheckStatus::Warning,
            "The OpenGL query failed.",
            "Check the display session, GLX availability, and graphics libraries."
        );
        return;
    }

    const QRegularExpression rendererRegex(
        R"(OpenGL renderer string:\s*(.+))",
        QRegularExpression::CaseInsensitiveOption
    );

    const QRegularExpressionMatch match =
        rendererRegex.match(output);

    if (!match.hasMatch())
    {
        addResult(
            report,
            "OpenGL",
            CheckStatus::Warning,
            "OpenGL information was returned, but the renderer was not identified.",
            "Review the glxinfo -B output and check GLX support."
        );
        return;
    }

    const QString renderer = match.captured(1).trimmed();

    // These are useful heuristics, not definitive proof.
    const bool softwareRenderer =
        renderer.contains("llvmpipe", Qt::CaseInsensitive) ||
        renderer.contains("softpipe", Qt::CaseInsensitive) ||
        renderer.contains("software rasterizer", Qt::CaseInsensitive);

    if (softwareRenderer)
    {
        addResult(
            report,
            "OpenGL hardware acceleration",
            CheckStatus::Warning,
            "OpenGL reports a software renderer: " + renderer,
            "If hardware acceleration is expected, inspect the active "
            "graphics driver and GLX/EGL configuration. Software rendering "
            "may be intentional in virtualized environments."
        );
    }
    else
    {
        addResult(
            report,
            "OpenGL renderer",
            CheckStatus::Passed,
            "OpenGL renderer: " + renderer,
            "Verify the renderer matches the GPU you expect applications to use."
        );
    }
}

void DiagnosticsEngine::checkVulkan(DiagnosticReport &report)
{
    if (!commandExists("vulkaninfo"))
    {
        addResult(
            report,
            "Vulkan",
            CheckStatus::NotChecked,
            "The vulkaninfo utility is not installed.",
            "Install it with: sudo apt install vulkan-tools"
        );
        return;
    }

    QProcess process;

    process.setProgram("vulkaninfo");
    process.setArguments({"--summary"});
    process.start();

    if (!process.waitForStarted(1500))
    {
        addResult(
            report,
            "Vulkan",
            CheckStatus::NotChecked,
            "Could not start vulkaninfo.",
            "Check the vulkan-tools installation."
        );
        return;
    }

    if (!process.waitForFinished(6000))
    {
        process.kill();
        process.waitForFinished(500);

        addResult(
            report,
            "Vulkan",
            CheckStatus::NotChecked,
            "The Vulkan query timed out.",
            "Check Vulkan initialization and the graphics driver configuration."
        );
        return;
    }

    const QString output =
        QString::fromLocal8Bit(process.readAllStandardOutput()) +
        QString::fromLocal8Bit(process.readAllStandardError());

    if (process.exitStatus() != QProcess::NormalExit ||
        process.exitCode() != 0)
    {
        addResult(
            report,
            "Vulkan",
            CheckStatus::Warning,
            "Vulkan device enumeration failed.",
            "Inspect the Vulkan loader, installed ICDs, and driver compatibility."
        );
        return;
    }

    const bool deviceFound =
        output.contains("GPU id", Qt::CaseInsensitive) ||
        output.contains("deviceName", Qt::CaseInsensitive) ||
        output.contains("device name", Qt::CaseInsensitive);

    if (deviceFound)
    {
        addResult(
            report,
            "Vulkan",
            CheckStatus::Passed,
            "Vulkan successfully returned physical-device information."
        );
    }
    else
    {
        addResult(
            report,
            "Vulkan",
            CheckStatus::Warning,
            "The Vulkan command completed, but no physical device was identified.",
            "Review the vulkaninfo output and installed Vulkan drivers."
        );
    }
}

void DiagnosticsEngine::calculateHealthScore(
    DiagnosticReport &report)
{
    report.passedCount = 0;
    report.warningCount = 0;
    report.failedCount = 0;
    report.notCheckedCount = 0;

    int applicableCount = 0;
    int earnedPoints = 0;

    for (const DiagnosticResult &check : report.checks)
    {
        switch (check.status)
        {
        case CheckStatus::Passed:
            ++report.passedCount;
            ++applicableCount;
            earnedPoints += 100;
            break;

        case CheckStatus::Warning:
            ++report.warningCount;
            ++applicableCount;
            earnedPoints += 50;
            break;

        case CheckStatus::Failed:
            ++report.failedCount;
            ++applicableCount;
            break;

        case CheckStatus::NotChecked:
            ++report.notCheckedCount;
            break;
        }
    }

    if (applicableCount == 0)
    {
        report.healthScore = -1;
        report.overallStatus = "Insufficient information";
        return;
    }

    report.healthScore = earnedPoints / applicableCount;

    if (report.failedCount > 0)
    {
        report.overallStatus = "Problems detected";
    }
    else if (report.warningCount > 0)
    {
        report.overallStatus = "Attention recommended";
    }
    else
    {
        report.overallStatus = "Checks passed";
    }
}

DiagnosticReport DiagnosticsEngine::runDiagnostics()
{
    DiagnosticReport report;

    checkGraphicsDevices(report);
    checkOpenGL(report);
    checkVulkan(report);

    calculateHealthScore(report);

    return report;
}

