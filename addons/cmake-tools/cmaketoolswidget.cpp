#include "cmaketoolswidget.h"

#include <KTextEditor/MainWindow>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>

CMakeToolsWidget::CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainwindow)
{
    setupUi(this);
    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &CMakeToolsWidget::checkCMakeListsFolder);
    connect(selectBfolder, &QToolButton::clicked, this, &CMakeToolsWidget::cmakeToolsBuildDir);
    connect(configCMakeToolsPlugin, &QPushButton::clicked, this, &CMakeToolsWidget::cmakeToolsGenLink);
}

CMakeToolsWidget::~CMakeToolsWidget() = default;

void CMakeToolsWidget::setSourceToBuildMap(const QMap<QString, QString> readedQMap)
{
    m_sourceToBuildMap = readedQMap;
}

const QMap<QString, QString> CMakeToolsWidget::getSourceToBuildMap()
{
    return m_sourceToBuildMap;
}

void CMakeToolsWidget::checkCMakeListsFolder(KTextEditor::View *v)
{
    auto getDocumentPath = [&v]() {
        return v->document()->url().path();
    };

    QString iterPath = getDocumentPath();
    iterPath.truncate(iterPath.lastIndexOf(QLatin1Char('/')));

    if (!sourceDirectoryPath->text().isEmpty() && iterPath.contains(sourceDirectoryPath->text())) {
        return;
    }

    QString comparePath = getDocumentPath();
    comparePath.truncate(comparePath.lastIndexOf(QLatin1Char('/'), 0));

    QString lastPath;

    while (iterPath != comparePath) {
        if (!QFileInfo(iterPath + QStringLiteral("/CMakeLists.txt")).exists()) {
            break;
        }
        lastPath = iterPath;
        iterPath.truncate(iterPath.lastIndexOf(QLatin1Char('/')));
    }

    sourceDirectoryPath->setReadOnly(false);
    sourceDirectoryPath->setText(lastPath);
    sourceDirectoryPath->setReadOnly(true);

    if (!m_sourceToBuildMap.contains(lastPath)) {
        return;
    }

    buildDirectoryPath->setText(m_sourceToBuildMap[lastPath]);
    return;
}

void CMakeToolsWidget::getSourceDirFromCMakeCache(){
    QFile openCMakeCache(buildDirectoryPath->text() + QStringLiteral("/CMakeCache.txt"));
    openCMakeCache.open(QIODevice::ReadWrite);
    QTextStream in (&openCMakeCache);
    const QString contents = in.readAll();
    const QString sourcePrefix = QStringLiteral("SOURCE_DIR:STATIC=");
    const int startSourcePathIndex = contents.indexOf(sourcePrefix);
    const int endSourcePathIndex = contents.indexOf(QStringLiteral("\n"), startSourcePathIndex);
    QString sourcePath = QStringLiteral("");
    for(int i = startSourcePathIndex + 18; i!=endSourcePathIndex; i++){
        sourcePath += contents[i];
    }
    openCMakeCache.close();
}

void CMakeToolsWidget::cmakeToolsBuildDir()
{
    if (sourceDirectoryPath->text().isEmpty()) {
        return;
    }
    const QString prefix = QFileDialog::getExistingDirectory(this, i18n("Get build folder"),
                                                             sourceDirectoryPath->text(), QFileDialog::ShowDirsOnly);
    if (prefix.isEmpty()) {
        return;
    }

    buildDirectoryPath->setText(prefix);
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsCheckifConfigured(const QString sourceCompileCommandsJsonpath, const QString buildCompileCommandsJsonpath)
{
    if (QFileInfo(buildCompileCommandsJsonpath).exists() && QFileInfo(sourceCompileCommandsJsonpath).symLinkTarget() == buildCompileCommandsJsonpath) {
        QMessageBox::information(this, i18n("Plugin already configured"), i18n("The plugin is already configured for this project"));
        m_sourceToBuildMap[sourceDirectoryPath->text()] = buildDirectoryPath->text();
        return CMakeRunStatus::Failure;
    }

    if (buildDirectoryPath->text().isEmpty() || sourceDirectoryPath->text().isEmpty()) {
        QMessageBox::warning(this,
                             i18n("Source or build folder path empty"),
                             i18n("The source or build folder path was not selected, please select a valid path. The source path is automatically selected "
                                  "when opening a file, the build path must be selected manually"));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsVerifyAndCreateCommands_Compilejson(const QString buildCompileCommandsJsonpath)
{
    QProcess cmakeProcess;
    int cmakeProcessReturn;

    if (QFileInfo(buildCompileCommandsJsonpath).exists()) {
        return CMakeRunStatus::Success;
    }

    QString buildCMakeCachetxtPath = buildDirectoryPath->text() + QStringLiteral("/CMakeCache.txt");

    if (!QFileInfo(buildCMakeCachetxtPath).exists()) {
        QMessageBox::warning(this,
                             i18n("Warning"),
                             i18n("File CMakeCache.txt not present on folder ") + i18n(buildDirectoryPath->text().toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    if (QMessageBox::Yes != QMessageBox::question(this, i18n("Generate file?"), i18n("Do you wish to generate the file compile_commands.json?"))) {
        return CMakeRunStatus::Failure;
    }

    cmakeProcess.setWorkingDirectory(buildDirectoryPath->text());
    cmakeProcessReturn =
        cmakeProcess.execute(QStringLiteral("cmake"), QStringList{buildDirectoryPath->text(), QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")});

    if (!cmakeProcess.waitForFinished(-1) && cmakeProcessReturn != 0) {
        QMessageBox::warning(this,
                             i18n("Warning"),
                             i18n("Failed to generate the compile_commands.json file ") + i18n("\nCMake return code: %1").arg(cmakeProcessReturn));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

CMakeRunStatus CMakeToolsWidget::cmakeToolsCreateLink(const QString sourceCompileCommandsJsonpath, const QString buildCompileCommandsJsonpath)
{
    if (QFileInfo(sourceCompileCommandsJsonpath).exists()) {
        QFile(sourceCompileCommandsJsonpath).remove();
    }

    QFile orig(buildCompileCommandsJsonpath);

    if (!orig.link(sourceCompileCommandsJsonpath)) {
        QMessageBox::warning(this, i18n("Warning"), i18n("Failed to create link in ") + i18n(sourceDirectoryPath->text().toStdString().c_str()));
        return CMakeRunStatus::Failure;
    }

    return CMakeRunStatus::Success;
}

void CMakeToolsWidget::cmakeToolsGenLink()
{
    const QString sourceCompileCommandsJsonpath = sourceDirectoryPath->text() + QStringLiteral("/compile_commands.json");
    const QString buildCompileCommandsJsonpath = buildDirectoryPath->text() + QStringLiteral("/compile_commands.json");

    CMakeRunStatus createReturn;

    createReturn = cmakeToolsCheckifConfigured(sourceCompileCommandsJsonpath, buildCompileCommandsJsonpath);

    if (createReturn == CMakeRunStatus::Failure) {
        return;
    }

    createReturn = cmakeToolsVerifyAndCreateCommands_Compilejson(buildCompileCommandsJsonpath);

    if (createReturn == CMakeRunStatus::Failure) {
        return;
    }

    createReturn = cmakeToolsCreateLink(sourceCompileCommandsJsonpath, buildCompileCommandsJsonpath);

    if (createReturn == CMakeRunStatus::Failure) {
        return;
    }

    QMessageBox::information(this,
                             i18n("Success"),
                             i18n("The plugin was configured successfully in ") + i18n(sourceDirectoryPath->text().toStdString().c_str()));

    m_sourceToBuildMap[sourceDirectoryPath->text()] = buildDirectoryPath->text();
}
