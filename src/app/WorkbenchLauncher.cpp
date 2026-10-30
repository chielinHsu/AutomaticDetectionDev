// Qt Creator executable target; the model runtime stays in a separate process (ADR-037).
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTextStream>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Vision CAD Studio"));
    const QStringList arguments = app.arguments().mid(1);
    const bool check = arguments.contains(QStringLiteral("--check"));
    const auto fail = [check](const QString& message) {
        QTextStream(stderr) << message << Qt::endl;
        if (!check) QMessageBox::critical(nullptr, QStringLiteral("Vision CAD Studio"), message);
        return 2;
    };
    const QDir payload(QCoreApplication::applicationDirPath() + QStringLiteral("/vision-workbench"));
    QFile manifest(payload.filePath(QStringLiteral("runtime.json")));
    if (!manifest.open(QIODevice::ReadOnly))
        return fail(QStringLiteral("Missing workbench payload. Build AiInspTool.pro with AOI_BUILD_PCB_WORKBENCH=ON."));
    const auto runtime = QJsonDocument::fromJson(manifest.readAll()).object();
    const QString python = runtime.value(QStringLiteral("python")).toString();
    if (runtime.value(QStringLiteral("format")).toString() != QStringLiteral("vision-cad-runtime")
        || runtime.value(QStringLiteral("version")).toInt() != 1 || !QFileInfo::exists(python)
        || !QFileInfo::exists(payload.filePath(QStringLiteral("tools/pcb_dl/cli.py"))))
        return fail(QStringLiteral("Invalid runtime or missing Python. Restore the environment and rebuild the workbench."));
    auto environment = QProcessEnvironment::systemEnvironment();
    // Native kit plugin paths must not override the PySide6 distribution's Qt plugins.
    for (const auto& key : {"QT_PLUGIN_PATH", "QT_QPA_PLATFORM_PLUGIN_PATH", "QML2_IMPORT_PATH", "PYTHONHOME", "PYTHONPATH"})
        environment.remove(QString::fromLatin1(key));
    environment.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    environment.insert(QStringLiteral("PYTHONNOUSERSITE"), QStringLiteral("1"));
    environment.insert(QStringLiteral("VISION_CAD_STATE_DIR"), payload.filePath(QStringLiteral("state")));
    QStringList child{QStringLiteral("-u"), QStringLiteral("-m"), QStringLiteral("tools.pcb_dl.cli")};
    if (check) {
        QStringList forwarded = arguments;
        forwarded.removeAll(QStringLiteral("--check"));
        child << QStringLiteral("check") << forwarded;
    }
    else child << QStringLiteral("gui") << arguments;
    QProcess process;
    process.setWorkingDirectory(payload.absolutePath());
    process.setProcessEnvironment(environment);
    process.setProcessChannelMode(QProcess::ForwardedChannels);
    process.start(python, child);
    if (!process.waitForStarted(10000)) return fail(process.errorString());
    process.waitForFinished(-1);
    if (process.exitStatus() != QProcess::NormalExit) return fail(QStringLiteral("The workbench process exited unexpectedly."));
    return process.exitCode();
}
