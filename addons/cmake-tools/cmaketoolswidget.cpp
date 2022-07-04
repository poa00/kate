#include "cmaketoolswidget.h"

#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QLineEdit>
#include <qstringliteral.h>

#include <KLocalizedString>
#include <memory>

CMakeToolsWidget::CMakeToolsWidget(KTextEditor::MainWindow *mainwindow, CMakeToolsPlugin *plugin, QWidget *parent)
    : QWidget(parent)
    , m_mainWindow(mainwindow)
    , m_plugin(plugin)
    , m_cmakeProcess(this)
{
    setupUi(this);
    m_cmakeProcess.setProcessChannelMode(QProcess::MergedChannels);
    cmakeExecutionLog->document()->setDefaultFont(KTextEditor::Editor::instance()->font());
    buildDirPath->lineEdit()->setPlaceholderText(QStringLiteral("Choose a build folder"));

    connect(m_mainWindow, &KTextEditor::MainWindow::viewChanged, this, &CMakeToolsWidget::guessCMakeListsDir);
    connect(selectBuildFolderBtn, &QToolButton::clicked, this, &CMakeToolsWidget::selectBuildFolder);
    connect(configureCMakeToolsBtn, &QPushButton::clicked, this, &CMakeToolsWidget::onConfigureBtnClicked);
    connect(&m_cmakeProcess, &QProcess::readyReadStandardOutput, this, &CMakeToolsWidget::appendCMakeProcessOutput);
    connect(&m_cmakeProcess, &QProcess::finished, this, &CMakeToolsWidget::cmakeFinished);
    connect(&m_cmakeProcess, &QProcess::errorOccurred, this, &CMakeToolsWidget::qProcessError);
}

CMakeToolsWidget::~CMakeToolsWidget()
{
    // ensure to kill, we segfault otherwise
    m_cmakeProcess.kill();
    m_cmakeProcess.waitForFinished();
}

void CMakeToolsWidget::setSourceToBuildMap(const QMap<QString, QStringList> &readedQMap)
{
    m_sourceToBuildMap = readedQMap;
}

QMap<QString, QStringList> CMakeToolsWidget::getSourceToBuildMap()
{
    return m_sourceToBuildMap;
}

void CMakeToolsWidget::loadWidgetSession(const QString &sourcePath)
{
    buildDirPath->clear();
    buildDirPath->insertItems(0, m_sourceToBuildMap[sourcePath]);
}

void CMakeToolsWidget::saveWidgetSession(const QString &sourcePath, const QString &buildPath)
{
    if (m_sourceToBuildMap[sourcePath].contains(buildPath)) {
        return;
    }
    m_sourceToBuildMap[sourcePath] << buildPath;

    loadWidgetSession(sourcePath);
}

void CMakeToolsWidget::guessCMakeListsDir(KTextEditor::View *v)
{
    bool docIsEmpty = v->document()->url().isEmpty();
    sourceLabel->setVisible(docIsEmpty);
    sourceDirPath->setVisible(docIsEmpty);

    if (docIsEmpty) {
        return;
    }

    QString currentViewDocPath = v->document()->url().path();
    if (!sourceDirPath->text().isEmpty() && currentViewDocPath.contains(sourceDirPath->text())) {
        return;
    }

    QDir currentDir = QFileInfo(currentViewDocPath).absoluteDir();
    QDir previousDir = currentDir;

    while (!currentDir.exists(QStringLiteral("CMakeLists.txt")) && currentDir.cdUp()) {
        previousDir = currentDir;
    }

    sourceDirPath->setText(previousDir.canonicalPath());
    searchBuildDir();
}

bool CMakeToolsWidget::containsCmakeCacheFile(const QString &buildPathToCheck)
{
    return QFileInfo::exists(buildPathToCheck + QStringLiteral("/CMakeCache.txt"));
}

QString CMakeToolsWidget::getSourceDirFromCMakeCache()
{
    if (!containsCmakeCacheFile(buildDirPath->lineEdit()->text())) {
        m_plugin->sendMessage(i18n("Folder not selected or the file CMakeCache.txt is not present on the folder selected"), true);
        return QLatin1String("");
    }

    const QString sourcePrefix = QStringLiteral("SOURCE_DIR:STATIC=");
    QFile cmakeCache(buildDirPath->lineEdit()->text() + QStringLiteral("/CMakeCache.txt"));
    cmakeCache.open(QIODevice::ReadOnly);
    QTextStream in(&cmakeCache);
    const QString lines = in.readAll();
    cmakeCache.close();

    if (!lines.contains(sourcePrefix)) {
        m_plugin->sendMessage(i18n("CMakeCache.txt does not contain path to source directory"), true);
        return QLatin1String("");
    }

    const int start = lines.indexOf(sourcePrefix) + sourcePrefix.length();
    const int end = lines.indexOf(QLatin1Char('\n'), start + 1);
    QString sourceFolderPath = lines.mid(start, end - start);
    return sourceFolderPath;
}

void CMakeToolsWidget::searchBuildDir()
{
    const QStringList possibleBuildPaths = QDir(sourceDirPath->text()).entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &possibleBuildPath : possibleBuildPaths) {
        QString path = sourceDirPath->text() + QStringLiteral("/") + possibleBuildPath;

        if (containsCmakeCacheFile(path)) {
            saveWidgetSession(sourceDirPath->text(), path);
        }
    }
}

void CMakeToolsWidget::selectBuildFolder()
{
    QString startingDir = QDir::homePath();
    if (!sourceDirPath->text().isEmpty()) {
        startingDir = sourceDirPath->text();
    }

    const QString selectedBuildPath = QFileDialog::getExistingDirectory(this, i18n("Choose a build folder"), startingDir, QFileDialog::ShowDirsOnly);

    if (selectedBuildPath.isEmpty()) {
        return;
    }

    buildDirPath->lineEdit()->setText(selectedBuildPath);
    sourceDirPath->setText(getSourceDirFromCMakeCache());
    searchBuildDir();
    loadWidgetSession(sourceDirPath->text());
}

void CMakeToolsWidget::appendCMakeProcessOutput()
{
    QString convertedOutput = QString::fromUtf8(m_cmakeProcess.readAllStandardOutput());
    cmakeExecutionLog->appendPlainText(convertedOutput);
}

bool CMakeToolsWidget::isCmakeToolsValid()
{
    if (sourceDirPath->text().isEmpty()) {
        m_plugin->sendMessage(i18n("Please select a valid path for the source folder by opening a non-empty document."), true);
        return false;
    }

    if (buildDirPath->lineEdit()->text().isEmpty()) {
        m_plugin->sendMessage(i18n("Please select a valid path for the build folder."), true);
        return false;
    }

    if (!containsCmakeCacheFile(buildDirPath->lineEdit()->text())) {
        m_plugin->sendMessage(i18n("The selected build folder does not contain a CMakeCache.txt file."), true);
        return false;
    }

    return true;
}

bool CMakeToolsWidget::copyCompileCommandsToSource(const QString &sourceCompileCommandsPath, const QString &buildCompileCommandsPath)
{
    if (QFileInfo::exists(sourceCompileCommandsPath)) {
        QFile(sourceCompileCommandsPath).remove();
    }

    QFile oldCompileCommands(buildCompileCommandsPath);

    if (!oldCompileCommands.copy(sourceCompileCommandsPath)) {
        m_plugin->sendMessage(i18n("Failed to copy %1 to %2", buildCompileCommandsPath, sourceCompileCommandsPath), true);
        return false;
    }

    return true;
}

void CMakeToolsWidget::qProcessError(QProcess::ProcessError error)
{
    Q_UNUSED(error);

    m_plugin->sendMessage(i18n("Failed to generate the compile_commands.json file"), true);
    configureCMakeToolsBtn->setEnabled(true);
}

void CMakeToolsWidget::onConfigureBtnClicked()
{
    configureCMakeToolsBtn->setDisabled(true);
    m_createCopyCheckBoxState = checkBoxCreateCopyOnSource->isChecked();

    if (!isCmakeToolsValid()) {
        configureCMakeToolsBtn->setEnabled(true);
        return;
    }

    m_cmakeProcess.setWorkingDirectory(buildDirPath->lineEdit()->text());
    m_cmakeProcess.start(QStringLiteral("cmake"), QStringList{buildDirPath->lineEdit()->text(), QStringLiteral("-DCMAKE_EXPORT_COMPILE_COMMANDS=ON")});
}

void CMakeToolsWidget::cmakeFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitCode)

    const QString sourceCompileCommandsPath = sourceDirPath->text() + QStringLiteral("/compile_commands.json");
    const QString buildCompileCommandsPath = buildDirPath->lineEdit()->text() + QStringLiteral("/compile_commands.json");

    if (exitStatus == QProcess::CrashExit) {
        qProcessError(m_cmakeProcess.error());
        return;
    }

    configureCMakeToolsBtn->setEnabled(true);
    if (m_createCopyCheckBoxState && !copyCompileCommandsToSource(sourceCompileCommandsPath, buildCompileCommandsPath)) {
        return;
    }

    saveWidgetSession(sourceDirPath->text(), buildDirPath->lineEdit()->text());
}
